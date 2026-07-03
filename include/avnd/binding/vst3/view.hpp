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
 * Ticks: on Windows the pugl timer is serviced by the host's message pump.
 * On Linux the host's IRunLoop (queried from the IPlugFrame) drives both a
 * periodic timer and the X11 connection's event handler. macOS pends on the
 * Cocoa blit pass.
 *
 * Known v1 limit: the message bus is wired to the *view-side* model, not to
 * the actual processor component -- routing it across the
 * processor/controller split needs IMessage (connection points).
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
    return Steinberg::kResultOk;
  }

  tresult removed() override
  {
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

  Controller& controller;
  avnd::effect_container<T> model;
  std::unique_ptr<avnd::ui_editor_t<T>> editor;
  Steinberg::IPlugFrame* frame{};
  double m_scale{1.};
};
}
