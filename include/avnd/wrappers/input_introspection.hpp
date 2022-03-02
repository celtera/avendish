#pragma once
#include <avnd/common/index_sequence.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <avnd/wrappers/concepts.hpp>
#include <avnd/wrappers/input_introspection.hpp>
#include <avnd/wrappers/output_introspection.hpp>
#include <boost/pfr.hpp>

namespace avnd
{

template <typename T>
struct parameter_input_introspection
    : parameter_introspection<typename inputs_type<T>::type> {};

template <typename T>
struct linear_timed_parameter_input_introspection
    : linear_timed_parameter_introspection<typename inputs_type<T>::type> {};

template <typename T>
struct span_timed_parameter_input_introspection
    : span_timed_parameter_introspection<typename inputs_type<T>::type> {};

template <typename T>
struct dynamic_timed_parameter_input_introspection
    : dynamic_timed_parameter_introspection<typename inputs_type<T>::type> {};

template <typename T>
struct midi_input_introspection : midi_port_introspection<typename inputs_type<T>::type> {};

template <typename T>
struct texture_input_introspection
    : texture_port_introspection<typename inputs_type<T>::type> {};

template <typename T>
struct raw_container_midi_input_introspection
    : raw_container_midi_port_introspection<typename inputs_type<T>::type> {};

template <typename T>
struct dynamic_container_midi_input_introspection
    : dynamic_container_midi_port_introspection<typename inputs_type<T>::type> {};

template <typename T>
struct audio_bus_input_introspection
    : audio_bus_introspection<typename inputs_type<T>::type> {};

template <typename T>
struct audio_channel_input_introspection
    : audio_channel_introspection<typename inputs_type<T>::type> {};

template <typename T>
struct input_introspection : fields_introspection<typename inputs_type<T>::type> {};

template <typename T>
auto&& get_inputs(avnd::effect_container<T>& t)
{
  return t.inputs();
}
template <avnd::inputs_is_type T>
auto& get_inputs(T& t)
{
  AVND_ERROR(T, "Cannot get inputs on T");
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
