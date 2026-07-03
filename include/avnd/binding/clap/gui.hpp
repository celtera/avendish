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

#include <avnd/common/no_unique_address.hpp>
#include <avnd/concepts/gui_window.hpp>
#include <avnd/concepts/ui.hpp>
#include <avnd/introspection/layout.hpp>

#include <clap/all.h>
#include <concurrentqueue.h>

#if defined(AVND_CLAP_UI)
#include <avnd/binding/ui/soft/window.hpp>
#endif

#include <functional>
#include <memory>
#include <type_traits>

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

// ---- Message-bus transport ----
// Extract the message type carried by a std::function<void(Msg)> member
// (T::send_message / ui::bus::send_message).
template <typename F>
struct function_arg;
template <typename R, typename A>
struct function_arg<std::function<R(A)>>
{
  using type = std::remove_cvref_t<A>;
};

namespace detail
{
template <typename T>
static constexpr auto ui_to_proc_msg()
{
  if constexpr(avnd::has_gui_to_processor_bus<T>)
    return std::type_identity<typename function_arg<std::remove_cvref_t<
        decltype(std::declval<typename T::ui::bus>().send_message)>>::type>{};
  else
    return std::type_identity<void>{};
}

template <typename T>
static constexpr auto proc_to_ui_msg()
{
  if constexpr(avnd::has_processor_to_gui_bus<T>)
    return std::type_identity<typename function_arg<
        std::remove_cvref_t<decltype(std::declval<T>().send_message)>>::type>{};
  else
    return std::type_identity<void>{};
}
}

template <typename T>
using ui_to_proc_msg_t = typename decltype(detail::ui_to_proc_msg<T>())::type;
template <typename T>
using proc_to_ui_msg_t = typename decltype(detail::proc_to_ui_msg<T>())::type;

struct no_queue
{
};

// UI thread enqueues (enqueue: may allocate, not RT), audio thread drains
// (try_dequeue: lock-free).
template <typename Msg>
struct ui_to_proc_queue
{
  moodycamel::ConcurrentQueue<Msg> queue{128};
};

// Audio thread enqueues: preallocated capacity + try_enqueue so the RT
// thread never allocates queue storage (messages beyond capacity are
// dropped; the message payload's own move is on the author). UI thread
// drains on the editor timer.
template <typename Msg>
struct proc_to_ui_queue
{
  moodycamel::ConcurrentQueue<Msg> queue{1024};
};

// GUI state carried by SimpleAudioEffect when an editor exists.
template <typename T, typename Editor>
struct gui_state
{
  std::unique_ptr<Editor> editor;
  gesture_queue gestures;
  clap_id timer_id{CLAP_INVALID_ID};
  double scale{1.};

  AVND_NO_UNIQUE_ADDRESS
  std::conditional_t<
      avnd::has_gui_to_processor_bus<T>, ui_to_proc_queue<ui_to_proc_msg_t<T>>,
      no_queue>
      to_processor;

  AVND_NO_UNIQUE_ADDRESS
  std::conditional_t<
      avnd::has_processor_to_gui_bus<T>, proc_to_ui_queue<proc_to_ui_msg_t<T>>,
      no_queue>
      to_ui;
};

struct no_gui_state
{
};
}
