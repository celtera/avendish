#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/for_nth.hpp>
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/audio_processor.hpp>
#include <avnd/concepts/processor.hpp>
#include <avnd/introspection/messages.hpp>

/**
 * This file allows to adapt hosts where the audio inputs are bus-based
 * to Avendish effects.
 *
 * By convention, the audio bus will be the first one.
 */
namespace avnd
{
// TODO be more fine-grained
template <typename T>
concept audio_arg_input = avnd::sample_arg_processor<T> || avnd::channel_arg_processor<
    T> || avnd::bus_arg_processor<T>;

template <typename T>
concept audio_arg_output = avnd::sample_arg_processor<T> || avnd::channel_arg_processor<
    T> || avnd::bus_arg_processor<T>;

template <typename T>
consteval int total_input_count()
{
  if (audio_arg_input<T>)
    return avnd::inputs_type<T>::size + 1;
  else
    return avnd::inputs_type<T>::size;
}

template <typename T>
consteval int total_output_count()
{
  if (audio_arg_output<T>)
    return avnd::outputs_type<T>::size + 1;
  else
    return avnd::outputs_type<T>::size;
}

// !! Important, keep this in sync with safe_node constructor order
template <typename T>
void port_visit_dispatcher(auto&& func_inlets, auto&& func_outlets)
{
  // Handle "virtual" audio ports for simple processors
  // which pass things by arguments

  if constexpr (avnd::mono_per_sample_arg_processor<double, T>)
  {
    struct
    {
      static consteval auto name() { return "Audio In"; }
      double sample;
    } fake_in;
    struct
    {
      static consteval auto name() { return "Audio Out"; }
      double sample;
    } fake_out;
    func_inlets(fake_in, avnd::field_index<0>{});
    func_outlets(fake_out, avnd::field_index<0>{});
  }
  else if constexpr (avnd::mono_per_sample_arg_processor<float, T>)
  {
    struct
    {
      static consteval auto name() { return "Audio In"; }
      float sample;
    } fake_in;
    struct
    {
      static consteval auto name() { return "Audio Out"; }
      float sample;
    } fake_out;
    func_inlets(fake_in, avnd::field_index<0>{});
    func_outlets(fake_out, avnd::field_index<0>{});
  }
  else if constexpr (avnd::mono_per_channel_arg_processor<double, T>)
  {
    struct
    {
      static consteval auto name() { return "Audio In"; }
      double* channel;
    } fake_in;
    struct
    {
      static consteval auto name() { return "Audio Out"; }
      double* channel;
    } fake_out;
    func_inlets(fake_in, avnd::field_index<0>{});
    func_outlets(fake_out, avnd::field_index<0>{});
  }
  else if constexpr (avnd::mono_per_channel_arg_processor<float, T>)
  {
    struct
    {
      static consteval auto name() { return "Audio In"; }
      float* channel;
    } fake_in;
    struct
    {
      static consteval auto name() { return "Audio Out"; }
      float* channel;
    } fake_out;
    func_inlets(fake_in, avnd::field_index<0>{});
    func_outlets(fake_out, avnd::field_index<0>{});
  }
  else if constexpr (avnd::polyphonic_arg_audio_effect<double, T>)
  {
    struct
    {
      static consteval auto name() { return "Audio In"; }
      double** samples;
    } fake_in;
    struct
    {
      static consteval auto name() { return "Audio Out"; }
      double** samples;
    } fake_out;
    func_inlets(fake_in, avnd::field_index<0>{});
    func_outlets(fake_out, avnd::field_index<0>{});
  }
  else if constexpr (avnd::polyphonic_arg_audio_effect<float, T>)
  {
    struct
    {
      static consteval auto name() { return "Audio In"; }
      float** samples;
    } fake_in;
    struct
    {
      static consteval auto name() { return "Audio Out"; }
      float** samples;
    } fake_out;
    func_inlets(fake_in, avnd::field_index<0>{});
    func_outlets(fake_out, avnd::field_index<0>{});
  }

  // Handle message inputs
  if constexpr (avnd::messages_type<T>::size > 0)
  {
    avnd::messages_introspection<T>::for_all([&](auto m) { func_inlets(m); });
  }

  if constexpr (avnd::has_inputs<T>)
  {
    using inputs_t = typename avnd::inputs_type<T>::type;
    inputs_t ins;
    avnd::for_each_field_ref_n(ins, func_inlets);
  }
  if constexpr (avnd::has_outputs<T>)
  {
    using outputs_t = typename avnd::outputs_type<T>::type;
    outputs_t outs;
    avnd::for_each_field_ref_n(outs, func_outlets);
  }
}
}
