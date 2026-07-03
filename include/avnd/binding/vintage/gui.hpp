#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * VST2 editor glue (CUSTOM_UI_PLAN.md §7.1): maps the vintage EditGetRect /
 * EditOpen / EditClose / EditIdle opcodes onto an avnd::gui_windowed_ui
 * editor (author-provided T::ui::window, or the soft pugl editor for
 * declarative UIs). See binding/ui/editor.hpp for editor resolution.
 *
 * Parameter flow: the editor writes controls directly and mirrors them into
 * the atomic Controls<> array (which the audio thread applies each block via
 * controls.write()); automation gestures go to the host through
 * audioMaster BeginEdit / Automate / EndEdit. Bus messages ride the shared
 * lock-free transport: UI→processor drained at the start of process(),
 * processor→UI drained on the editor's 30 Hz frame hook and on EditIdle.
 */

#include <avnd/binding/ui/editor.hpp>
#include <avnd/binding/ui/message_transport.hpp>
#include <avnd/binding/vintage/vintage.hpp>
#include <avnd/common/no_unique_address.hpp>
#include <avnd/wrappers/controls.hpp>

#include <memory>

namespace vintage
{
template <typename T>
struct gui_state
{
  std::unique_ptr<avnd::ui_editor_t<T>> editor;
  vintage::Rect rect{};
  AVND_NO_UNIQUE_ADDRESS avnd::bus_transport<T> bus;
};

struct no_gui_state
{
};

template <typename T>
using gui_state_t
    = std::conditional_t<avnd::has_ui_editor<T>, gui_state<T>, no_gui_state>;

// ---- Host gesture protocol (UI thread) ----
template <typename Self>
static avnd::gui_host gui_make_host(Self& self)
{
  using T = typename Self::effect_type;
  avnd::gui_host h;
  h.ctx = &self;
  h.begin_edit = [](void* ctx, int param) {
    auto& s = *static_cast<Self*>(ctx);
    s.request(HostOpcodes::BeginEdit, param, 0, nullptr, 0.f);
  };
  h.perform_edit = [](void* ctx, int param, double normalized) {
    auto& s = *static_cast<Self*>(ctx);
    // Mirror into the atomics so the audio thread's controls.write() applies
    // the change instead of stomping it with the previous value.
    if constexpr(requires { s.controls.parameters[param]; })
      if(param >= 0
         && param < std::decay_t<decltype(s.controls)>::parameter_count)
        s.controls.parameters[param].store(
            (float)normalized, std::memory_order_release);
    s.request(HostOpcodes::Automate, param, 0, nullptr, (float)normalized);
  };
  h.end_edit = [](void* ctx, int param) {
    auto& s = *static_cast<Self*>(ctx);
    s.request(HostOpcodes::EndEdit, param, 0, nullptr, 0.f);
  };
  return h;
}

// ---- Editor lifecycle ----
template <typename Self>
static void gui_drain_to_ui(Self& self)
{
  using T = typename Self::effect_type;
  if constexpr(avnd::has_processor_to_gui_bus<T>)
  {
    if constexpr(requires { self.gui.editor->runtime(); })
    {
      if(self.gui.editor)
      {
        avnd::proc_to_ui_msg_t<T> msg;
        while(self.gui.bus.to_ui.queue.try_dequeue(msg))
          self.gui.editor->runtime().deliver_to_ui(std::move(msg));
      }
    }
  }
}

// Called from the audio thread at the start of each block.
template <typename Self>
static void gui_drain_to_processor(Self& self)
{
  using T = typename Self::effect_type;
  if constexpr(avnd::has_ui_editor<T> && avnd::has_gui_to_processor_bus<T>)
  {
    if constexpr(requires {
                   self.effect.effect.process_message(
                       std::declval<avnd::ui_to_proc_msg_t<T>>());
                 })
    {
      avnd::ui_to_proc_msg_t<T> msg;
      while(self.gui.bus.to_processor.queue.try_dequeue(msg))
        self.effect.effect.process_message(std::move(msg));
    }
  }
}

// processor → UI: always enqueue (send_message must be callable whether or
// not an editor is open). Re-installed after editor creation because the
// soft runtime's constructor wires a synchronous default.
template <typename Self>
static void gui_wire_bus(Self& self)
{
  using T = typename Self::effect_type;
  if constexpr(avnd::has_ui_editor<T> && avnd::has_processor_to_gui_bus<T>)
  {
    if constexpr(requires { self.effect.effect.send_message = nullptr; })
    {
      self.effect.effect.send_message = [&self](auto&& msg) {
        self.gui.bus.to_ui.queue.try_enqueue(std::forward<decltype(msg)>(msg));
      };
    }
  }
}

// Hosts may call EditGetRect before EditOpen: create the editor on demand.
template <typename Self>
static void gui_ensure(Self& self)
{
  using T = typename Self::effect_type;
  if constexpr(avnd::has_ui_editor<T>)
  {
    if(self.gui.editor)
      return;
    self.gui.editor = avnd::make_ui_editor(self.effect);

    if constexpr(requires { self.gui.editor->runtime(); })
    {
      if constexpr(avnd::has_gui_to_processor_bus<T>)
      {
        self.gui.editor->runtime().set_bus_to_processor([&self](auto&& msg) {
          self.gui.bus.to_processor.queue.enqueue(
              std::forward<decltype(msg)>(msg));
        });
      }
      gui_wire_bus(self);
    }
    if constexpr(requires { self.gui.editor->on_frame; })
    {
      self.gui.editor->on_frame = [&self] { gui_drain_to_ui(self); };
    }
  }
}

// ---- Opcode handlers, called from default_dispatch ----
template <typename Self>
static intptr_t gui_dispatch(
    Self& self, EffectOpcodes opcode, int32_t index, intptr_t value, void* ptr,
    float opt)
{
  using T = typename Self::effect_type;
  if constexpr(!avnd::has_ui_editor<T>)
  {
    return 0;
  }
  else
  {
    switch(opcode)
    {
      case EffectOpcodes::EditGetRect: {
        if(!ptr)
          return 0;
        gui_ensure(self);
        const auto [w, h] = self.gui.editor->size();
        self.gui.rect
            = {.top = 0, .left = 0, .bottom = int16_t(h), .right = int16_t(w)};
        *reinterpret_cast<vintage::Rect**>(ptr) = &self.gui.rect;
        return 1;
      }

      case EffectOpcodes::EditOpen: {
        gui_ensure(self);
        avnd::gui_parent parent{.handle = ptr, .scale = 1.};
#if defined(_WIN32)
        parent.api = avnd::gui_api::win32_hwnd;
#elif defined(__APPLE__)
        parent.api = avnd::gui_api::cocoa_nsview;
#else
        parent.api = avnd::gui_api::x11_window;
#endif
        self.gui.editor->open(parent, gui_make_host(self));
        return 1;
      }

      case EffectOpcodes::EditClose: {
        if(self.gui.editor)
        {
          self.gui.editor->close();
          self.gui.editor.reset();
        }
        return 1;
      }

      case EffectOpcodes::EditIdle: {
        if(self.gui.editor)
        {
          gui_drain_to_ui(self);
          self.gui.editor->idle();
        }
        return 1;
      }

      default:
        return 0;
    }
  }
}
}
