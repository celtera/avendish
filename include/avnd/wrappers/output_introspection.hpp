#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/index_sequence.hpp>
#include <avnd/wrappers/concepts.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/port_introspection.hpp>
#include <boost/pfr.hpp>

namespace avnd
{
template <typename T>
struct parameter_output_introspection
    : parameter_introspection<typename outputs_type<T>::type> {};

template <typename T>
struct control_output_introspection
    : control_introspection<typename outputs_type<T>::type> {};

template <typename T>
struct linear_timed_parameter_output_introspection
    : linear_timed_parameter_introspection<typename outputs_type<T>::type> {};

template <typename T>
struct span_timed_parameter_output_introspection
    : span_timed_parameter_introspection<typename outputs_type<T>::type> {};

template <typename T>
struct dynamic_timed_parameter_output_introspection
    : dynamic_timed_parameter_introspection<typename outputs_type<T>::type> {};

template <typename T>
struct midi_output_introspection
    : midi_port_introspection<typename outputs_type<T>::type> {};

template <typename T>
struct raw_container_midi_output_introspection
    : raw_container_midi_port_introspection<typename outputs_type<T>::type> {};

template <typename T>
struct dynamic_container_midi_output_introspection
    : dynamic_container_midi_port_introspection<typename outputs_type<T>::type> {};

template <typename T>
struct texture_output_introspection
    : texture_port_introspection<typename outputs_type<T>::type> {};

template <typename T>
struct audio_bus_output_introspection
    : audio_bus_introspection<typename outputs_type<T>::type> {};

template <typename T>
struct audio_channel_output_introspection
    : audio_channel_introspection<typename outputs_type<T>::type> {};

template <typename T>
struct callback_output_introspection
    : callback_introspection<typename outputs_type<T>::type> {};

template <typename T>
struct output_introspection : fields_introspection<typename outputs_type<T>::type> {};

template <typename T>
auto get_outputs(avnd::effect_container<T>& t)
  -> decltype(t.outputs())
{
  return t.outputs();
}
template <avnd::outputs_is_type T>
auto& get_outputs(T& t)
{
  AVND_ERROR(T, "Cannot get outputs on T");
}
template <avnd::outputs_is_value T>
auto& get_outputs(T& t)
{
  return t.outputs;
}

template <typename T>
static constexpr void for_all_outputs(T& obj, auto&& func) noexcept
{
  if constexpr (output_introspection<T>::size > 0)
  {
    boost::pfr::for_each_field(get_outputs(obj), func);
  }
}
}
