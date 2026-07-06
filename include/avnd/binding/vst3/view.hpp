#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * VST3 IPlugView glue (CUSTOM_UI_PLAN.md §7.2), driving an
 * avnd::gui_windowed_ui editor (author-provided T::ui::window or the soft
 * pugl editor). See binding/ui/editor.hpp for editor resolution.
 *
 * The view owns a controller-side model instance (effect_container<T>) that
 * the editor renders and mutates; host automation reaches it through
 * Controller::setParamNormalized -> ui_param_changed, and user edits reach
 * the host through IComponentHandler begin/perform/endEdit with the same
 * normalized mapping as getParamNormalized.
 *
 * Ticks: the view owns a 16 ms UI-thread timer while attached (Win32
 * SetTimer / host IRunLoop on Linux / CFRunLoopTimer on macOS). Each tick
 * calls the editor's idle() (this is the clock Tier C editors are promised
 * by the concept; the soft editor's pugl timer coexists harmlessly) and
 * pumps the processor->UI message bus (see binding/vst3/bus.hpp).
 */

#include <avnd/binding/ui/editor.hpp>
#include <avnd/binding/vst3/bus.hpp>
#include <avnd/binding/vst3/refcount.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/effect_container.hpp>

#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/gui/iplugviewcontentscalesupport.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>

#include <memory>
#include <string_view>

#if defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#include <map>
#elif defined(__APPLE__)
#include <CoreFoundation/CFRunLoop.h>
#endif

namespace stv3
{
template <typename T, typename Controller>
class plug_view final
    : public Steinberg::IPlugView
    , public Steinberg::IPlugViewContentScaleSupport
    , public stv3::refcount
{
  using tresult = Steinberg::tresult;
  using ViewRect = Steinberg::ViewRect;
  using ParamID = Steinberg::Vst::ParamID;
  using inputs_info_t = avnd::parameter_input_introspection<T>;

public:
  explicit plug_view(Controller& ctrl)
      : controller{ctrl}
  {
    // addRef/release are private overrides in the controller; go through
    // the public interface names.
    controller_iface().addRef();

    if constexpr(avnd::has_inputs<T>)
    {
      avnd::init_controls(model);
      // Adopt the controller's current parameter state
      for(auto state : model.full_state())
      {
        if constexpr(requires {
                       state.inputs = controller.inputs_mirror;
                     })
          state.inputs = controller.inputs_mirror;
      }
    }

    editor = avnd::make_ui_editor(model);
    controller.ui_param_changed
        = [this](ParamID tag, double value) { apply_param(tag, value); };

    // Message bus: route through the host connection to the real
    // processing component instead of the view-local model
    // (binding/vst3/bus.hpp; trivially-copyable payloads only for now).
    if constexpr(stv3::bus_to_processor_enabled<T>)
    {
      if constexpr(requires { editor->runtime(); })
      {
        editor->runtime().set_bus_to_processor(
            [this](auto&& msg) { controller.send_ui_message(msg); });
      }
      else if constexpr(requires { editor->send_message; })
      {
        editor->send_message
            = [this](auto&& msg) { controller.send_ui_message(msg); };
      }
    }
    if constexpr(stv3::bus_to_ui_enabled<T>)
    {
      controller.ui_message_received = [this](auto msg) {
        if(!editor)
          return;
        if constexpr(requires { editor->runtime(); })
          editor->runtime().deliver_to_ui(std::move(msg));
        else if constexpr(requires { editor->process_message(std::move(msg)); })
          editor->process_message(std::move(msg));
      };
    }
  }

  ~plug_view()
  {
    stop_tick();
    controller.ui_param_changed = {};
    if constexpr(stv3::bus_to_ui_enabled<T>)
      controller.ui_message_received = {};
    if(editor)
      editor->close();
    editor.reset();
    if(frame)
      frame->release();
    controller_iface().release();
  }

  Steinberg::Vst::IEditController& controller_iface() noexcept
  {
    return controller;
  }

  plug_view(const plug_view&) = delete;
  plug_view& operator=(const plug_view&) = delete;

  // ---- FUnknown ----
  tresult queryInterface(const Steinberg::TUID iid, void** obj) override
  {
    QUERY_INTERFACE(iid, obj, Steinberg::FUnknown::iid, Steinberg::IPlugView);
    QUERY_INTERFACE(iid, obj, Steinberg::IPlugView::iid, Steinberg::IPlugView);
    QUERY_INTERFACE(
        iid, obj, Steinberg::IPlugViewContentScaleSupport::iid,
        Steinberg::IPlugViewContentScaleSupport);
    *obj = nullptr;
    return Steinberg::kNoInterface;
  }
  Steinberg::uint32 addRef() override { return ++m_refcount; }
  Steinberg::uint32 release() override
  {
    if(--m_refcount == 0)
    {
      delete this;
      return 0;
    }
    return m_refcount;
  }

  // ---- IPlugView ----
  tresult isPlatformTypeSupported(Steinberg::FIDString type) override
  {
    if(!type)
      return Steinberg::kInvalidArgument;
    const std::string_view t = type;
#if defined(_WIN32)
    if(t == Steinberg::kPlatformTypeHWND)
      return Steinberg::kResultTrue;
#elif defined(__APPLE__)
    if(t == Steinberg::kPlatformTypeNSView)
      return Steinberg::kResultTrue;
#else
    if(t == Steinberg::kPlatformTypeX11EmbedWindowID)
      return Steinberg::kResultTrue;
#endif
    return Steinberg::kResultFalse;
  }

  tresult attached(void* parent, Steinberg::FIDString type) override
  {
    if(isPlatformTypeSupported(type) != Steinberg::kResultTrue)
      return Steinberg::kInvalidArgument;
    if(!editor)
      return Steinberg::kResultFalse;

    avnd::gui_parent p{.handle = parent, .scale = m_scale};
#if defined(_WIN32)
    p.api = avnd::gui_api::win32_hwnd;
#elif defined(__APPLE__)
    p.api = avnd::gui_api::cocoa_nsview;
#else
    p.api = avnd::gui_api::x11_window;
#endif
    editor->open(p, make_host());
    start_tick();
    return Steinberg::kResultOk;
  }

  tresult removed() override
  {
    stop_tick();
    if(editor)
      editor->close();
    return Steinberg::kResultOk;
  }

  tresult onWheel(float) override { return Steinberg::kResultFalse; }
  tresult onKeyDown(Steinberg::char16, Steinberg::int16, Steinberg::int16) override
  {
    return Steinberg::kResultFalse;
  }
  tresult onKeyUp(Steinberg::char16, Steinberg::int16, Steinberg::int16) override
  {
    return Steinberg::kResultFalse;
  }

  tresult getSize(ViewRect* size) override
  {
    if(!size || !editor)
      return Steinberg::kInvalidArgument;
    const auto [w, h] = editor->size();
    *size = ViewRect{0, 0, (Steinberg::int32)w, (Steinberg::int32)h};
    return Steinberg::kResultTrue;
  }

  tresult onSize(ViewRect*) override { return Steinberg::kResultTrue; }
  tresult onFocus(Steinberg::TBool) override { return Steinberg::kResultTrue; }

  tresult setFrame(Steinberg::IPlugFrame* f) override
  {
    if(frame)
      frame->release();
    frame = f;
    if(frame)
      frame->addRef();
    return Steinberg::kResultTrue;
  }

  tresult canResize() override { return Steinberg::kResultFalse; }

  // ---- IPlugViewContentScaleSupport ----
  tresult setContentScaleFactor(
      Steinberg::IPlugViewContentScaleSupport::ScaleFactor factor) override
  {
    m_scale = factor > 0.f ? factor : 1.f;
    if constexpr(requires { editor->set_scale(m_scale); })
    {
      if(editor)
      {
        editor->set_scale(m_scale);
        return Steinberg::kResultTrue;
      }
    }
    return Steinberg::kResultFalse;
  }

  tresult checkSizeConstraint(ViewRect* rect) override
  {
    if(!rect || !editor)
      return Steinberg::kInvalidArgument;
    const auto [w, h] = editor->size();
    rect->right = rect->left + w;
    rect->bottom = rect->top + h;
    return Steinberg::kResultTrue;
  }

private:
  avnd::gui_host make_host()
  {
    avnd::gui_host h;
    h.ctx = this;
    h.begin_edit = [](void* ctx, int param) {
      auto& self = *static_cast<plug_view*>(ctx);
      if(auto* handler = self.controller.componentHandler;
         handler && param >= 0 && param < inputs_info_t::size)
        handler->beginEdit(inputs_info_t::index_map[param]);
    };
    h.perform_edit = [](void* ctx, int param, double normalized) {
      auto& self = *static_cast<plug_view*>(ctx);
      if(param < 0 || param >= inputs_info_t::size)
        return;
      const ParamID tag = inputs_info_t::index_map[param];
      // Keep the controller mirror current so getParamNormalized is fresh
      if constexpr(avnd::has_inputs<T>)
      {
        inputs_info_t::for_nth_raw(
            self.controller.inputs_mirror, tag, [&]<typename C>(C& field) {
              if constexpr(requires { avnd::map_control_from_01<C>(normalized); })
                assign_if_assignable(
                    field.value, avnd::map_control_from_01<C>(normalized));
            });
      }
      if(auto* handler = self.controller.componentHandler)
        handler->performEdit(tag, normalized);
    };
    h.end_edit = [](void* ctx, int param) {
      auto& self = *static_cast<plug_view*>(ctx);
      if(auto* handler = self.controller.componentHandler;
         handler && param >= 0 && param < inputs_info_t::size)
        handler->endEdit(inputs_info_t::index_map[param]);
    };
    return h;
  }

  // Host automation / preset load -> update the view-side model
  void apply_param(ParamID tag, double normalized)
  {
    if constexpr(avnd::has_inputs<T>)
    {
      for(auto state : model.full_state())
      {
        inputs_info_t::for_nth_raw(state.inputs, tag, [&]<typename C>(C& field) {
          if constexpr(requires { avnd::map_control_from_01<C>(normalized); })
            assign_if_assignable(
                field.value, avnd::map_control_from_01<C>(normalized));
        });
      }
      if constexpr(requires { editor->runtime(); })
        if(editor)
          editor->runtime().mark_dirty();
    }
  }

  // ---- UI-thread tick: editor idle() + processor->UI bus pump ----
  static constexpr int tick_ms = 16;

  void on_tick()
  {
    if(editor)
      editor->idle();
    if constexpr(stv3::bus_to_ui_enabled<T>)
      controller.send_bus_pump();
  }

#if defined(_WIN32)
  // HWND-less SetTimer: the TimerProc trampoline resolves the view through
  // a UI-thread-only id map.
  static std::map<UINT_PTR, plug_view*>& timer_map()
  {
    static std::map<UINT_PTR, plug_view*> m;
    return m;
  }
  static void CALLBACK timer_proc(HWND, UINT, UINT_PTR id, DWORD)
  {
    auto& m = timer_map();
    if(auto it = m.find(id); it != m.end())
      it->second->on_tick();
  }
  void start_tick()
  {
    if(m_timer)
      return;
    m_timer = SetTimer(nullptr, 0, tick_ms, &plug_view::timer_proc);
    if(m_timer)
      timer_map()[m_timer] = this;
  }
  void stop_tick()
  {
    if(m_timer)
    {
      KillTimer(nullptr, m_timer);
      timer_map().erase(m_timer);
      m_timer = 0;
    }
  }
  UINT_PTR m_timer{};
#elif defined(__APPLE__)
  static void timer_cb(CFRunLoopTimerRef, void* info)
  {
    static_cast<plug_view*>(info)->on_tick();
  }
  void start_tick()
  {
    if(m_timer)
      return;
    CFRunLoopTimerContext ctx{};
    ctx.info = this;
    m_timer = CFRunLoopTimerCreate(
        nullptr, CFAbsoluteTimeGetCurrent() + tick_ms / 1000., tick_ms / 1000., 0,
        0, &plug_view::timer_cb, &ctx);
    if(m_timer)
      CFRunLoopAddTimer(CFRunLoopGetMain(), m_timer, kCFRunLoopCommonModes);
  }
  void stop_tick()
  {
    if(m_timer)
    {
      CFRunLoopTimerInvalidate(m_timer);
      CFRelease(m_timer);
      m_timer = nullptr;
    }
  }
  CFRunLoopTimerRef m_timer{};
#else
  // Host-provided run loop (queried from the IPlugFrame, per the VST3
  // Linux windowing contract).
  struct timer_handler final : Steinberg::Linux::ITimerHandler
  {
    plug_view* self{};
    tresult queryInterface(const Steinberg::TUID iid, void** obj) override
    {
      QUERY_INTERFACE(
          iid, obj, Steinberg::FUnknown::iid, Steinberg::Linux::ITimerHandler);
      QUERY_INTERFACE(
          iid, obj, Steinberg::Linux::ITimerHandler::iid,
          Steinberg::Linux::ITimerHandler);
      *obj = nullptr;
      return Steinberg::kNoInterface;
    }
    // Lifetime is the owning view's
    Steinberg::uint32 addRef() override { return 2; }
    Steinberg::uint32 release() override { return 1; }
    void onTimer() override { self->on_tick(); }
  };
  void start_tick()
  {
    if(m_runloop || !frame)
      return;
    Steinberg::Linux::IRunLoop* rl{};
    if(frame->queryInterface(Steinberg::Linux::IRunLoop::iid, (void**)&rl)
           == Steinberg::kResultOk
       && rl)
    {
      m_tick_handler.self = this;
      rl->registerTimer(&m_tick_handler, tick_ms);
      m_runloop = rl;
    }
  }
  void stop_tick()
  {
    if(m_runloop)
    {
      m_runloop->unregisterTimer(&m_tick_handler);
      m_runloop->release();
      m_runloop = nullptr;
    }
  }
  timer_handler m_tick_handler;
  Steinberg::Linux::IRunLoop* m_runloop{};
#endif

  Controller& controller;
  avnd::effect_container<T> model;
  std::unique_ptr<avnd::ui_editor_t<T>> editor;
  Steinberg::IPlugFrame* frame{};
  double m_scale{1.};
};
}
