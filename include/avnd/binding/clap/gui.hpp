#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * clap.gui + clap.timer-support glue (CUSTOM_UI_PLAN.md §7.3).
 *
 * Editor resolution, in order:
 *  - T::ui::window (avnd::gui_windowed_ui): author brings any framework;
 *  - otherwise, when built with the soft UI backend (AVND_CLAP_UI), a
 *    declarative `struct ui` gets the reference pugl+Nuklear+canvas_ity
 *    editor;
 *  - otherwise no editor: the extension is not exposed.
 *
 * Automation gestures flow: editor (UI thread) -> gesture queue ->
 * host params request_flush -> params.flush emits
 * CLAP_EVENT_PARAM_GESTURE_BEGIN / PARAM_VALUE / GESTURE_END.
 */

#include <avnd/concepts/gui_window.hpp>
#include <avnd/introspection/layout.hpp>

#include <clap/all.h>

#if defined(AVND_CLAP_UI)
#include <avnd/binding/ui/soft/window.hpp>
#endif

#include <memory>
#include <mutex>
#include <vector>

namespace avnd_clap
{
template <typename T>
concept has_layout_ui = requires { sizeof(typename avnd::ui_type<T>::type); };

namespace detail
{
template <typename T>
static constexpr auto editor_type()
{
  if constexpr(avnd::has_gui_window<T>)
    return std::type_identity<typename T::ui::window>{};
#if defined(AVND_CLAP_UI)
  else if constexpr(has_layout_ui<T>)
    return std::type_identity<avnd::soft_ui::window_editor<T>>{};
#endif
  else
    return std::type_identity<void>{};
}
}

template <typename T>
using clap_editor_t = typename decltype(detail::editor_type<T>())::type;

template <typename T>
static constexpr bool clap_has_editor = !std::is_void_v<clap_editor_t<T>>;

#if defined(_WIN32)
static constexpr const char* clap_native_window_api = CLAP_WINDOW_API_WIN32;
static constexpr avnd::gui_api native_gui_api = avnd::gui_api::win32_hwnd;
#elif defined(__APPLE__)
static constexpr const char* clap_native_window_api = CLAP_WINDOW_API_COCOA;
static constexpr avnd::gui_api native_gui_api = avnd::gui_api::cocoa_nsview;
#else
static constexpr const char* clap_native_window_api = CLAP_WINDOW_API_X11;
static constexpr avnd::gui_api native_gui_api = avnd::gui_api::x11_window;
#endif

// Pending automation-gesture events, filled on the UI thread, drained by
// params.flush (main or audio thread).
struct gesture_event
{
  enum kind : uint8_t
  {
    begin,
    end,
    value
  } type{};
  clap_id param_id{};
  double value_mapped{}; // clap value space (map_control_to_double)
};

struct gesture_queue
{
  // A mutex is fine here: contention is UI-rate on one side and
  // flush-rate on the other, both tiny. Lock-free refinement can come with
  // the SPSC bus transport.
  std::mutex mutex;
  std::vector<gesture_event> pending;

  void push(gesture_event ev)
  {
    std::lock_guard _{mutex};
    pending.push_back(ev);
  }

  template <typename F>
  void drain(F&& f)
  {
    std::vector<gesture_event> local;
    {
      std::lock_guard _{mutex};
      std::swap(local, pending);
    }
    for(auto& ev : local)
      f(ev);
  }
};

// GUI state carried by SimpleAudioEffect when an editor exists.
template <typename Editor>
struct gui_state
{
  std::unique_ptr<Editor> editor;
  gesture_queue gestures;
  clap_id timer_id{CLAP_INVALID_ID};
  double scale{1.};
};

struct no_gui_state
{
};
}
