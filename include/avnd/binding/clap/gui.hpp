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

#include <avnd/binding/ui/editor.hpp>
#include <avnd/binding/ui/message_transport.hpp>
#include <avnd/common/no_unique_address.hpp>
#include <avnd/concepts/gui_window.hpp>
#include <avnd/concepts/ui.hpp>
#include <avnd/introspection/layout.hpp>

#include <clap/all.h>
#include <concurrentqueue.h>

#include <functional>
#include <memory>
#include <type_traits>

namespace avnd_clap
{
template <typename T>
using clap_editor_t = avnd::ui_editor_t<T>;

template <typename T>
static constexpr bool clap_has_editor = avnd::has_ui_editor<T>;

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
  // Single producer (UI thread), so moodycamel's per-producer FIFO keeps
  // begin/value/end ordered. enqueue() may allocate: fine on the UI thread.
  moodycamel::ConcurrentQueue<gesture_event> queue{128};

  void push(gesture_event ev) { queue.enqueue(ev); }

  template <typename F>
  void drain(F&& f)
  {
    gesture_event ev;
    while(queue.try_dequeue(ev))
      f(ev);
  }
};

// ---- Message-bus transport: shared queue machinery ----
template <typename T>
using ui_to_proc_msg_t = avnd::ui_to_proc_msg_t<T>;
template <typename T>
using proc_to_ui_msg_t = avnd::proc_to_ui_msg_t<T>;

// GUI state carried by SimpleAudioEffect when an editor exists.
template <typename T, typename Editor>
struct gui_state
{
  std::unique_ptr<Editor> editor;
  gesture_queue gestures;
  clap_id timer_id{CLAP_INVALID_ID};
  double scale{1.};

  AVND_NO_UNIQUE_ADDRESS avnd::bus_transport<T> bus;
};

struct no_gui_state
{
};
}
