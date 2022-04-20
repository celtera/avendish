#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/index_sequence.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <boost/pfr.hpp>

namespace avnd
{

template <typename T>
struct parameter_input_introspection
    : parameter_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct control_input_introspection : control_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct linear_timed_parameter_input_introspection
    : linear_timed_parameter_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct span_timed_parameter_input_introspection
    : span_timed_parameter_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct dynamic_timed_parameter_input_introspection
    : dynamic_timed_parameter_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct midi_input_introspection : midi_port_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct texture_input_introspection
    : texture_port_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct cpu_texture_input_introspection
    : cpu_texture_port_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct gpu_sampler_introspection
    : gpu_sampler_port_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct gpu_image_input_introspection
        : gpu_image_port_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct gpu_uniform_introspection
        : gpu_uniform_port_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct raw_container_midi_input_introspection
    : raw_container_midi_port_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct dynamic_container_midi_input_introspection
    : dynamic_container_midi_port_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct audio_bus_input_introspection
    : audio_bus_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct audio_channel_input_introspection
    : audio_channel_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct soundfile_input_introspection
    : soundfile_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
struct input_introspection : fields_introspection<typename inputs_type<T>::type>
{
};

template <typename T>
auto get_inputs(avnd::effect_container<T>& t) -> decltype(t.inputs())
{
  return t.inputs();
}
template <avnd::inputs_is_type T>
auto& get_inputs(T& t)
{
#if !AVND_UNIQUE_INSTANCE
  AVND_ERROR(T, "Cannot get inputs on T");
#else
  static typename T::inputs r;
  return r;
#endif
}
template <avnd::inputs_is_value T>
auto& get_inputs(T& t)
{
  return t.inputs;
}
}
