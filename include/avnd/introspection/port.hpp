#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/aggregates.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/all.hpp>

namespace avnd
{

/* Not possible until clang-13 :"(
 *
template<auto F, typename T>
using concept_to_mp_bool = boost::mp11::mp_bool<F.template operator()<T>()>;

#define CONCEPT(TheConcept) \
  [] <typename T> () consteval { return TheConcept<T>; }

static_assert(check<CONCEPT(std::floating_point), float>::value);
*/

#define AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(Concept)                    \
                                                                                \
  template <typename Field>                                                     \
  using is_##Concept##_t = boost::mp11::mp_bool<Concept<Field>>;                \
  template <typename T>                                                         \
  struct Concept##_introspection : predicate_introspection<T, is_##Concept##_t> \
  {                                                                             \
  };                                                                            \
  template <typename T>                                                         \
  struct Concept##_input_introspection                                          \
      : Concept##_introspection<typename inputs_type<T>::type>                  \
  {                                                                             \
  };                                                                            \
  template <typename T>                                                         \
  struct Concept##_output_introspection                                         \
      : Concept##_introspection<typename outputs_type<T>::type>                 \
  {                                                                             \
  };                                                                            \
                                                                                \
  template <typename Field>                                                     \
  concept check_dynamic_##Concept                                               \
      = Concept<Field>                                                          \
        || (dynamic_ports_port<Field>                                           \
            && Concept<typename decltype(Field::ports)::value_type>);           \
                                                                                \
  template <typename Field>                                                     \
  using is_dynamic_##Concept##_t                                                \
      = boost::mp11::mp_bool<check_dynamic_##Concept<Field>>;                   \
                                                                                \
  template <typename T>                                                         \
  struct dynamic_##Concept##_introspection                                      \
      : predicate_introspection<T, is_dynamic_##Concept##_t>                    \
  {                                                                             \
  };                                                                            \
  template <typename T>                                                         \
  struct dynamic_##Concept##_input_introspection                                \
      : dynamic_##Concept##_introspection<typename inputs_type<T>::type>        \
  {                                                                             \
  };                                                                            \
  template <typename T>                                                         \
  struct dynamic_##Concept##_output_introspection                               \
      : dynamic_##Concept##_introspection<typename outputs_type<T>::type>       \
  {                                                                             \
  };

AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(parameter_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(program_parameter)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(control_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(value_port)

template <typename Field>
using is_mapped_control_t
    = boost::mp11::mp_bool<control_port<Field> && has_mapper<Field>>;
template <typename T>
struct mapped_control_port_introspection
    : predicate_introspection<T, is_mapped_control_t>
{
};
template <typename T>
using mapped_control_input_introspection
    = mapped_control_port_introspection<typename inputs_type<T>::type>;
template <typename T>
using mapped_control_output_introspection
    = mapped_control_port_introspection<typename outputs_type<T>::type>;

AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(time_control_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(span_parameter_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(linear_sample_accurate_parameter_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(span_sample_accurate_parameter_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(dynamic_sample_accurate_parameter_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(smooth_parameter_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(midi_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(cpu_buffer_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(gpu_buffer_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(buffer_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(texture_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(cpu_texture_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(gpu_texture_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(matrix_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(sampler_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(image_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(attachment_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(uniform_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(audio_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(raw_container_midi_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(dynamic_container_midi_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(mono_audio_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(poly_audio_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(fixed_audio_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(callback)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(stored_callback)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(view_callback)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(soundfile_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(midifile_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(raw_file_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(file_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(curve_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(spectrum_split_channel_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(spectrum_complex_channel_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(spectrum_split_bus_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(spectrum_complex_bus_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(attribute_port)
AVND_PORT_INTROSPECTION_FOR_DYNAMIC_CONCEPT(geometry_port)

template <typename Field>
using is_dynamic_ports_port_t = boost::mp11::mp_bool<dynamic_ports_port<Field>>;
template <typename T>
struct dynamic_ports_port_introspection
    : predicate_introspection<T, is_dynamic_ports_port_t>
{
};
template <typename T>
using dynamic_ports_input_introspection
    = dynamic_ports_port_introspection<typename inputs_type<T>::type>;
template <typename T>
using dynamic_ports_output_introspection
    = dynamic_ports_port_introspection<typename outputs_type<T>::type>;

template <typename T>
using audio_bus_introspection = poly_audio_port_introspection<T>;
template <typename T>
using audio_bus_input_introspection
    = audio_bus_introspection<typename inputs_type<T>::type>;
template <typename T>
using audio_bus_output_introspection
    = audio_bus_introspection<typename outputs_type<T>::type>;

template <typename T>
using audio_channel_introspection = mono_audio_port_introspection<T>;
template <typename T>
using audio_channel_input_introspection
    = audio_channel_introspection<typename inputs_type<T>::type>;
template <typename T>
using audio_channel_output_introspection
    = audio_channel_introspection<typename outputs_type<T>::type>;

template <typename T>
using soundfile_input_introspection = soundfile_port_input_introspection<T>;
template <typename T>
using midifile_input_introspection = midifile_port_input_introspection<T>;

// clang-format off
template<typename T> using parameter_input_introspection = parameter_port_input_introspection<T>;
template<typename T> using control_input_introspection = control_port_input_introspection<T>;
template<typename T> using time_control_input_introspection = time_control_port_input_introspection<T>;
template<typename T> using span_parameter_input_introspection = span_parameter_port_input_introspection<T>;
template<typename T> using linear_sample_accurate_parameter_input_introspection = linear_sample_accurate_parameter_port_input_introspection<T>;
template<typename T> using linear_timed_parameter_input_introspection = linear_sample_accurate_parameter_port_input_introspection<T>;
template<typename T> using span_sample_accurate_parameter_input_introspection = span_sample_accurate_parameter_port_input_introspection<T>;
template<typename T> using span_timed_parameter_input_introspection = span_sample_accurate_parameter_port_input_introspection<T>;
template<typename T> using dynamic_sample_accurate_parameter_input_introspection = dynamic_sample_accurate_parameter_port_input_introspection<T>;
template<typename T> using dynamic_timed_parameter_input_introspection = dynamic_sample_accurate_parameter_port_input_introspection<T>;
template<typename T> using smooth_parameter_input_introspection = smooth_parameter_port_input_introspection<T>;
template<typename T> using midi_input_introspection = midi_port_input_introspection<T>;
template<typename T> using cpu_buffer_input_introspection = cpu_buffer_port_input_introspection<T>;
template<typename T> using gpu_buffer_input_introspection = gpu_buffer_port_input_introspection<T>;
template<typename T> using buffer_input_introspection = buffer_port_input_introspection<T>;
template<typename T> using texture_input_introspection = texture_port_input_introspection<T>;
template<typename T> using cpu_texture_input_introspection = cpu_texture_port_input_introspection<T>;
template<typename T> using gpu_texture_input_introspection = gpu_texture_port_input_introspection<T>;
template<typename T> using matrix_input_introspection = matrix_port_input_introspection<T>;
template<typename T> using gpu_matrix_input_introspection = matrix_port_input_introspection<T>;
template<typename T> using sampler_input_introspection = sampler_port_input_introspection<T>;
template<typename T> using gpu_sampler_input_introspection = sampler_port_input_introspection<T>;
template<typename T> using gpu_sampler_introspection = sampler_port_input_introspection<T>;
template<typename T> using image_input_introspection = image_port_input_introspection<T>;
template<typename T> using gpu_image_input_introspection = image_port_input_introspection<T>;
template<typename T> using attachment_input_introspection = attachment_port_input_introspection<T>;
template<typename T> using uniform_input_introspection = uniform_port_input_introspection<T>;
template<typename T> using gpu_uniform_input_introspection = uniform_port_input_introspection<T>;
template<typename T> using gpu_uniform_introspection = uniform_port_input_introspection<T>;
template<typename T> using audio_input_introspection = audio_port_input_introspection<T>;
template<typename T> using raw_container_midi_input_introspection = raw_container_midi_port_input_introspection<T>;
template<typename T> using dynamic_container_midi_input_introspection = dynamic_container_midi_port_input_introspection<T>;
template<typename T> using mono_audio_input_introspection = mono_audio_port_input_introspection<T>;
template<typename T> using poly_audio_input_introspection = poly_audio_port_input_introspection<T>;
template<typename T> using cal_input_introspection = callback_input_introspection<T>;
template<typename T> using stored_cal_input_introspection = stored_callback_input_introspection<T>;
template<typename T> using view_cal_input_introspection = view_callback_input_introspection<T>;
template<typename T> using soundfile_input_introspection = soundfile_port_input_introspection<T>;
template<typename T> using midifile_input_introspection = midifile_port_input_introspection<T>;
template<typename T> using raw_file_input_introspection = raw_file_port_input_introspection<T>;
template<typename T> using file_input_introspection = file_port_input_introspection<T>;
template<typename T> using curve_input_introspection = curve_port_input_introspection<T>;
template<typename T> using spectrum_split_channel_input_introspection = spectrum_split_channel_port_input_introspection<T>;
template<typename T> using spectrum_complex_channel_input_introspection = spectrum_complex_channel_port_input_introspection<T>;
template<typename T> using spectrum_split_bus_input_introspection = spectrum_split_bus_port_input_introspection<T>;
template<typename T> using spectrum_complex_bus_input_introspection = spectrum_complex_bus_port_input_introspection<T>;
template<typename T> using attribute_input_introspection = attribute_port_input_introspection<T>;
template<typename T> using geometry_input_introspection = geometry_port_input_introspection<T>;


template<typename T> using parameter_output_introspection = parameter_port_output_introspection<T>;
template<typename T> using control_output_introspection = control_port_output_introspection<T>;
template<typename T> using time_control_output_introspection = time_control_port_output_introspection<T>;
template<typename T> using span_parameter_output_introspection = span_parameter_port_output_introspection<T>;
template<typename T> using linear_sample_accurate_parameter_output_introspection = linear_sample_accurate_parameter_port_output_introspection<T>;
template<typename T> using linear_timed_parameter_output_introspection = linear_sample_accurate_parameter_port_output_introspection<T>;
template<typename T> using span_sample_accurate_parameter_output_introspection = span_sample_accurate_parameter_port_output_introspection<T>;
template<typename T> using span_timed_parameter_output_introspection = span_sample_accurate_parameter_port_output_introspection<T>;
template<typename T> using dynamic_sample_accurate_parameter_output_introspection = dynamic_sample_accurate_parameter_port_output_introspection<T>;
template<typename T> using dynamic_timed_parameter_output_introspection = dynamic_sample_accurate_parameter_port_output_introspection<T>;
template<typename T> using smooth_parameter_output_introspection = smooth_parameter_port_output_introspection<T>;
template<typename T> using midi_output_introspection = midi_port_output_introspection<T>;
template<typename T> using cpu_buffer_output_introspection = cpu_buffer_port_output_introspection<T>;
template<typename T> using gpu_buffer_output_introspection = gpu_buffer_port_output_introspection<T>;
template<typename T> using buffer_output_introspection = buffer_port_output_introspection<T>;
template<typename T> using texture_output_introspection = texture_port_output_introspection<T>;
template<typename T> using cpu_texture_output_introspection = cpu_texture_port_output_introspection<T>;
template<typename T> using gpu_texture_output_introspection = gpu_texture_port_output_introspection<T>;
template<typename T> using matrix_output_introspection = matrix_port_output_introspection<T>;
template<typename T> using gpu_matrix_output_introspection = matrix_port_output_introspection<T>;
template<typename T> using sampler_output_introspection = sampler_port_output_introspection<T>;
template<typename T> using gpu_sampler_output_introspection = sampler_port_output_introspection<T>;
template<typename T> using image_output_introspection = image_port_output_introspection<T>;
template<typename T> using gpu_image_output_introspection = image_port_output_introspection<T>;
template<typename T> using attachment_output_introspection = attachment_port_output_introspection<T>;
template<typename T> using gpu_attachment_output_introspection = attachment_port_output_introspection<T>;
template<typename T> using uniform_output_introspection = uniform_port_output_introspection<T>;
template<typename T> using audio_output_introspection = audio_port_output_introspection<T>;
template<typename T> using raw_container_midi_output_introspection = raw_container_midi_port_output_introspection<T>;
template<typename T> using dynamic_container_midi_output_introspection = dynamic_container_midi_port_output_introspection<T>;
template<typename T> using mono_audio_output_introspection = mono_audio_port_output_introspection<T>;
template<typename T> using poly_audio_output_introspection = poly_audio_port_output_introspection<T>;
template<typename T> using cal_output_introspection = callback_output_introspection<T>;
template<typename T> using stored_cal_output_introspection = stored_callback_output_introspection<T>;
template<typename T> using view_cal_output_introspection = view_callback_output_introspection<T>;
template<typename T> using soundfile_output_introspection = soundfile_port_output_introspection<T>;
template<typename T> using midifile_output_introspection = midifile_port_output_introspection<T>;
template<typename T> using raw_file_output_introspection = raw_file_port_output_introspection<T>;
template<typename T> using file_output_introspection = file_port_output_introspection<T>;
template<typename T> using curve_output_introspection = curve_port_output_introspection<T>;
template<typename T> using spectrum_split_channel_output_introspection = spectrum_split_channel_port_output_introspection<T>;
template<typename T> using spectrum_complex_channel_output_introspection = spectrum_complex_channel_port_output_introspection<T>;
template<typename T> using spectrum_split_bus_output_introspection = spectrum_split_bus_port_output_introspection<T>;
template<typename T> using spectrum_complex_bus_output_introspection = spectrum_complex_bus_port_output_introspection<T>;
template<typename T> using attribute_output_introspection = attribute_port_output_introspection<T>;
template<typename T> using geometry_output_introspection = geometry_port_output_introspection<T>;
//clang-format on

// template <typename Field>
// using is_message_t = boost::mp11::mp_bool<message<Field>>;
// template <typename T>
// using message_introspection = predicate_introspection<T, is_message_t>;
}
