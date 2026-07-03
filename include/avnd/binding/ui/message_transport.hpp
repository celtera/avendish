#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Lock-free message-bus transport shared by the plug-in bindings.
 *
 * The UI thread enqueues towards the processor with enqueue() (may allocate,
 * not RT); the audio thread enqueues towards the UI with try_enqueue() on a
 * preallocated queue (never allocates queue storage; overflow drops) and
 * drains with try_dequeue() (lock-free). Each binding decides where the
 * drains live (process() on the audio side, editor timer on the UI side).
 */

#include <avnd/common/no_unique_address.hpp>
#include <avnd/concepts/ui.hpp>

#include <concurrentqueue.h>

#include <functional>
#include <type_traits>

namespace avnd
{
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

template <typename Msg>
struct ui_to_proc_queue
{
  moodycamel::ConcurrentQueue<Msg> queue{128};
};

template <typename Msg>
struct proc_to_ui_queue
{
  moodycamel::ConcurrentQueue<Msg> queue{1024};
};

// The queue pair a binding embeds when T has a bus; empty otherwise.
template <typename T>
struct bus_transport
{
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
}
