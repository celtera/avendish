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

template <typename Field>
using is_parameter_t = boost::mp11::mp_bool<parameter<Field>>;
template <typename T>
struct parameter_introspection : predicate_introspection<T, is_parameter_t>
{
};

template <typename Field>
using is_program_parameter_t = boost::mp11::mp_bool<program_parameter<Field>>;
template <typename T>
struct program_parameter_introspection
    : predicate_introspection<T, is_program_parameter_t>
{
};

template <typename Field>
using is_control_t = boost::mp11::mp_bool<control<Field>>;
template <typename T>
struct control_introspection : predicate_introspection<T, is_control_t>
{
};

template <typename Field>
using is_value_port_t = boost::mp11::mp_bool<value_port<Field>>;
template <typename T>
struct value_port_introspection : predicate_introspection<T, is_value_port_t>
{
};

template <typename Field>
using is_mapped_control_t = boost::mp11::mp_bool<control<Field> && has_mapper<Field>>;
template <typename T>
struct mapped_control_introspection : predicate_introspection<T, is_mapped_control_t>
{
};

template <typename Field>
using is_time_control_t = boost::mp11::mp_bool<time_control<Field>>;
template <typename T>
struct time_control_introspection : predicate_introspection<T, is_time_control_t>
{
};

template <typename Field>
using is_span_parameter_t = boost::mp11::mp_bool<span_parameter<Field>>;
template <typename T>
using span_parameter_introspection = predicate_introspection<T, is_span_parameter_t>;

template <typename Field>
using is_linear_timed_parameter_t
    = boost::mp11::mp_bool<linear_sample_accurate_parameter<Field>>;
template <typename T>
using linear_timed_parameter_introspection
    = predicate_introspection<T, is_linear_timed_parameter_t>;

template <typename Field>
using is_span_timed_parameter_t
    = boost::mp11::mp_bool<span_sample_accurate_parameter<Field>>;
template <typename T>
using span_timed_parameter_introspection
    = predicate_introspection<T, is_span_timed_parameter_t>;

template <typename Field>
using is_dynamic_timed_parameter_t
    = boost::mp11::mp_bool<dynamic_sample_accurate_parameter<Field>>;
template <typename T>
using dynamic_timed_parameter_introspection
    = predicate_introspection<T, is_dynamic_timed_parameter_t>;

template <typename Field>
using is_smooth_parameter_t = boost::mp11::mp_bool<smooth_parameter<Field>>;
template <typename T>
using smooth_parameter_introspection = predicate_introspection<T, is_smooth_parameter_t>;

template <typename Field>
using is_dynamic_ports_port_t = boost::mp11::mp_bool<dynamic_ports_port<Field>>;
template <typename T>
struct dynamic_ports_port_introspection
    : predicate_introspection<T, is_dynamic_ports_port_t>
{
};

template <typename Field>
using is_midi_port_t = boost::mp11::mp_bool<midi_port<Field>>;
template <typename T>
struct midi_port_introspection : predicate_introspection<T, is_midi_port_t>
{
};

template <typename Field>
using is_buffer_port_t = boost::mp11::mp_bool<buffer_port<Field>>;
template <typename T>
using buffer_port_introspection = predicate_introspection<T, is_buffer_port_t>;

template <typename Field>
using is_texture_port_t = boost::mp11::mp_bool<texture_port<Field>>;
template <typename T>
using texture_port_introspection = predicate_introspection<T, is_texture_port_t>;

template <typename Field>
using is_cpu_texture_port_t = boost::mp11::mp_bool<cpu_texture_port<Field>>;
template <typename T>
using cpu_texture_port_introspection = predicate_introspection<T, is_cpu_texture_port_t>;

template <typename Field>
using is_matrix_port_t = boost::mp11::mp_bool<matrix_port<Field>>;
template <typename T>
using matrix_port_introspection = predicate_introspection<T, is_matrix_port_t>;

template <typename Field>
using is_gpu_sampler_port_t = boost::mp11::mp_bool<sampler_port<Field>>;
template <typename T>
using gpu_sampler_port_introspection = predicate_introspection<T, is_gpu_sampler_port_t>;

template <typename Field>
using is_gpu_image_port_t = boost::mp11::mp_bool<image_port<Field>>;
template <typename T>
using gpu_image_port_introspection = predicate_introspection<T, is_gpu_image_port_t>;

template <typename Field>
using is_gpu_attachment_port_t = boost::mp11::mp_bool<attachment_port<Field>>;
template <typename T>
using gpu_attachment_port_introspection
    = predicate_introspection<T, is_gpu_attachment_port_t>;

template <typename Field>
using is_gpu_uniform_port_t = boost::mp11::mp_bool<uniform_port<Field>>;
template <typename T>
using gpu_uniform_port_introspection = predicate_introspection<T, is_gpu_uniform_port_t>;

template <typename Field>
using is_audio_port_t = boost::mp11::mp_bool<audio_port<Field>>;
template <typename T>
struct audio_port_introspection : predicate_introspection<T, is_audio_port_t>
{
};

template <typename Field>
using is_raw_container_midi_port_t
    = boost::mp11::mp_bool<raw_container_midi_port<Field>>;
template <typename T>
using raw_container_midi_port_introspection
    = predicate_introspection<T, is_raw_container_midi_port_t>;

template <typename Field>
using is_dynamic_container_midi_port_t
    = boost::mp11::mp_bool<dynamic_container_midi_port<Field>>;
template <typename T>
using dynamic_container_midi_port_introspection
    = predicate_introspection<T, is_dynamic_container_midi_port_t>;

template <typename Field>
using is_audio_bus_t = boost::mp11::mp_bool<poly_audio_port<Field>>;
template <typename T>
using audio_bus_introspection = predicate_introspection<T, is_audio_bus_t>;

template <typename Field>
using is_audio_channel_t = boost::mp11::mp_bool<mono_audio_port<Field>>;
template <typename T>
using audio_channel_introspection = predicate_introspection<T, is_audio_channel_t>;

// template <typename Field>
// using is_message_t = boost::mp11::mp_bool<message<Field>>;
// template <typename T>
// using message_introspection = predicate_introspection<T, is_message_t>;

// Callbacks
template <typename Field>
using is_callback_t = boost::mp11::mp_bool<callback<Field>>;
template <typename T>
using callback_introspection = predicate_introspection<T, is_callback_t>;

template <typename Field>
using is_dynamic_callback_t = boost::mp11::mp_bool<dynamic_callback<Field>>;
template <typename T>
using dynamic_callback_introspection = predicate_introspection<T, is_dynamic_callback_t>;

template <typename Field>
using is_view_callback_t = boost::mp11::mp_bool<view_callback<Field>>;
template <typename T>
using view_callback_introspection = predicate_introspection<T, is_view_callback_t>;

// Soundfile
template <typename Field>
using is_soundfile_t = boost::mp11::mp_bool<soundfile_port<Field>>;
template <typename T>
using soundfile_introspection = predicate_introspection<T, is_soundfile_t>;

// Midifile
template <typename Field>
using is_midifile_t = boost::mp11::mp_bool<midifile_port<Field>>;
template <typename T>
using midifile_introspection = predicate_introspection<T, is_midifile_t>;

// Raw file
template <typename Field>
using is_raw_file_t = boost::mp11::mp_bool<raw_file_port<Field>>;
template <typename T>
using raw_file_introspection = predicate_introspection<T, is_raw_file_t>;

// Any kind of file
template <typename Field>
using is_file_t = boost::mp11::mp_bool<file_port<Field>>;
template <typename T>
using file_introspection = predicate_introspection<T, is_file_t>;

// Curves
template <typename Field>
using is_curve_t = boost::mp11::mp_bool<curve_port<Field>>;
template <typename T>
using curve_introspection = predicate_introspection<T, is_curve_t>;

// Attributes
template <typename Field>
using is_attribute_t = boost::mp11::mp_bool<attribute_port<Field>>;
template <typename T>
using attribute_introspection = predicate_introspection<T, is_attribute_t>;

// FFT
template <typename Field>
using is_spectrum_split_channel_port_t
    = boost::mp11::mp_bool<spectrum_split_channel_port<Field>>;
template <typename T>
using spectrum_split_channel_port_introspection
    = predicate_introspection<T, is_spectrum_split_channel_port_t>;

template <typename Field>
using is_spectrum_complex_channel_port_t
    = boost::mp11::mp_bool<spectrum_complex_channel_port<Field>>;
template <typename T>
using spectrum_complex_channel_port_introspection
    = predicate_introspection<T, is_spectrum_complex_channel_port_t>;

template <typename Field>
using is_spectrum_split_bus_port_t
    = boost::mp11::mp_bool<spectrum_split_bus_port<Field>>;
template <typename T>
using spectrum_split_bus_port_introspection
    = predicate_introspection<T, is_spectrum_split_bus_port_t>;

template <typename Field>
using is_spectrum_complex_bus_port_t
    = boost::mp11::mp_bool<spectrum_complex_bus_port<Field>>;
template <typename T>
using spectrum_complex_bus_port_introspection
    = predicate_introspection<T, is_spectrum_complex_bus_port_t>;

// Geometry
template <typename Field>
using is_geometry_t = boost::mp11::mp_bool<geometry_port<Field>>;
template <typename T>
using geometry_introspection = predicate_introspection<T, is_geometry_t>;
}
