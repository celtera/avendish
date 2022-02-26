#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/concepts.hpp>
#include <avnd/wrappers/port_introspection.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <boost/pfr.hpp>
#include <avnd/common/index_sequence.hpp>

namespace avnd
{
template <typename T>
using parameter_output_introspection
    = parameter_introspection<typename outputs_type<T>::type>;

template <typename T>
using midi_output_introspection
    = midi_port_introspection<typename outputs_type<T>::type>;

template <typename T>
using raw_container_midi_output_introspection
    = raw_container_midi_port_introspection<typename outputs_type<T>::type>;

template <typename T>
using audio_bus_output_introspection
    = audio_bus_introspection<typename outputs_type<T>::type>;

template <typename T>
using audio_channel_output_introspection
    = audio_channel_introspection<typename outputs_type<T>::type>;

template <typename T>
using callback_output_introspection
    = callback_introspection<typename outputs_type<T>::type>;

template <typename T>
using output_introspection
    = fields_introspection<typename outputs_type<T>::type>;

template <typename T>
auto& get_outputs(avnd::effect_container<T>& t)
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
