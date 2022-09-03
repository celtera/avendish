#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/aggregates.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/common/function_reflection.hpp>
#include <avnd/common/span_polyfill.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/wrappers/prepare.hpp>
#include <avnd/wrappers/process_execution.hpp>

#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include <vector>

namespace avnd
{

template <typename Fp>
struct zero_storage
{
  std::vector<Fp> zeros_in, zeros_out;
  std::vector<Fp*> zero_pointers_in, zero_pointers_out;
};

/**
 * This class is used to adapt between hosts that will send audio as arrays of float** / double** channels
 * to various useful cases
 */

// Default case when it's not an audio object - still
// bang it at regular intervals
template <typename T>
struct process_adapter
{
  template <std::floating_point SrcFP>
  void allocate_buffers(process_setup setup, SrcFP f)
  {
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation, avnd::span<FP*> in, avnd::span<FP*> out,
      const auto& tick)
  {
    // Audio buffers aren't used at all
    invoke_effect(implementation, tick);
  }
};

template <typename T>
struct audio_buffer_storage
{
  // buffers used in case we need to convert float -> double
  [[no_unique_address]] buffer_type<double, T> m_dsp_buffer_input_f;
  [[no_unique_address]] buffer_type<double, T> m_dsp_buffer_output_f;
  [[no_unique_address]] buffer_type<float, T> m_dsp_buffer_input_d;
  [[no_unique_address]] buffer_type<float, T> m_dsp_buffer_output_d;
  [[no_unique_address]] zero_storage<float> m_zero_storage_f;
  [[no_unique_address]] zero_storage<double> m_zero_storage_d;

  auto& input_buffer_for(float) { return m_dsp_buffer_input_f; }
  auto& input_buffer_for(double) { return m_dsp_buffer_input_d; }
  auto& output_buffer_for(float) { return m_dsp_buffer_output_f; }
  auto& output_buffer_for(double) { return m_dsp_buffer_output_d; }
  auto& zero_storage_for(float) { return m_zero_storage_f; }
  auto& zero_storage_for(double) { return m_zero_storage_d; }

  template <std::floating_point SrcFP>
  void allocate_buffers(process_setup setup, SrcFP f)
  {
    // If our effect is written with doubles, and we're in a host
    // which requires floats, we allocate buffers to store the converted data
    if constexpr(needs_storage<SrcFP, T>::value)
    {
      using needed_type = typename needs_storage<SrcFP, T>::needed_storage_t;
      input_buffer_for(needed_type{})
          .resize(setup.frames_per_buffer * setup.input_channels);
      output_buffer_for(needed_type{})
          .resize(setup.frames_per_buffer * setup.output_channels);
    }

    // Let's play it safe for the cases where the host does not supply
    // enough buffers
    auto& floats = zero_storage_for(float{});
    auto& doubles = zero_storage_for(double{});
    floats.zeros_in.resize(setup.frames_per_buffer);
    floats.zeros_out.resize(setup.frames_per_buffer);
    doubles.zeros_in.resize(setup.frames_per_buffer);
    doubles.zeros_out.resize(setup.frames_per_buffer);

    int max_channels_in = (16 + setup.input_channels) * 16;
    int max_channels_out = (16 + setup.output_channels) * 16;
    floats.zero_pointers_in.resize(max_channels_in);
    floats.zero_pointers_out.resize(max_channels_out);
    doubles.zero_pointers_in.resize(max_channels_in);
    doubles.zero_pointers_out.resize(max_channels_out);
    for(int i = 0; i < max_channels_in; i++)
    {
      floats.zero_pointers_in[i] = floats.zeros_in.data();
      doubles.zero_pointers_in[i] = doubles.zeros_in.data();
    }
    for(int i = 0; i < max_channels_out; i++)
    {
      floats.zero_pointers_out[i] = floats.zeros_out.data();
      doubles.zero_pointers_out[i] = doubles.zeros_out.data();
    }
  }
};
}
