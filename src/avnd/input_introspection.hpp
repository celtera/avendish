#pragma once
#include <avnd/concepts.hpp>
#include <avnd/effect_container.hpp>
#include <avnd/port_introspection.hpp>
#include <boost/pfr.hpp>
#include <common/index_sequence.hpp>
#include <common/struct_reflection.hpp>

namespace avnd
{

template <typename T>
using parameter_input_introspection
    = parameter_introspection<typename inputs_type<T>::type>;

template <typename T>
using float_parameter_input_introspection
    = float_parameter_introspection<typename inputs_type<T>::type>;

template <typename T>
using midi_input_introspection
    = midi_port_introspection<typename inputs_type<T>::type>;

template <typename T>
using raw_container_midi_input_introspection
    = raw_container_midi_port_introspection<typename inputs_type<T>::type>;

template <typename T>
using audio_bus_input_introspection
    = audio_bus_introspection<typename inputs_type<T>::type>;

template <typename T>
using audio_channel_input_introspection
    = audio_channel_introspection<typename inputs_type<T>::type>;

template <typename T>
using input_introspection
    = fields_introspection<typename inputs_type<T>::type>;

template <typename T>
void for_nth_parameter(
    avnd::effect_container<T>& object,
    int n,
    auto&& func) noexcept
{
  float_parameter_input_introspection<T>::for_nth(object.inputs(), n, func);
}

template <typename T>
auto& get_inputs(avnd::effect_container<T>& t)
{
  return t.inputs();
}
template <avnd::inputs_is_type T>
auto& get_inputs(T& t)
{
  static_assert(std::is_void_v<T>, "Cannot get inputs on T");
}
template <avnd::inputs_is_value T>
auto& get_inputs(T& t)
{
  return t.inputs;
}

// TODO generalize
template <typename T>
static constexpr void for_all_inputs(T& obj, auto&& func) noexcept
{
  if constexpr (input_introspection<T>::size > 0)
  {
    boost::pfr::for_each_field(get_inputs(obj), func);
  }
}
}
