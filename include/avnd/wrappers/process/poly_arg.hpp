#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/process/base.hpp>

namespace avnd
{

/**
 * Handles case where inputs / outputs are e.g. operator()(float** ins, float** outs)
 */
template <typename T>
  requires polyphonic_audio_processor<
               T> && (polyphonic_arg_audio_effect<double, T> || polyphonic_arg_audio_effect<float, T>)
struct process_adapter<T> : audio_buffer_storage<T>
{
  template <typename SrcFP, typename DstFP>
  void process_arg(
      avnd::effect_container<T>& implementation,
      avnd::span<SrcFP*> in,
      avnd::span<SrcFP*> out,
      int32_t n)
  {
    const int input_channels = in.size();
    const int output_channels = out.size();
    if constexpr (needs_storage<SrcFP, T>::value)
    {
      // Fetch the required temporary storage
      using needed_type = typename needs_storage<SrcFP, T>::needed_storage_t;
      auto& dsp_buffer_input = audio_buffer_storage<T>::input_buffer_for(needed_type{});
      auto& dsp_buffer_output
          = audio_buffer_storage<T>::output_buffer_for(needed_type{});

      // Convert inputs and outputs to double
      auto in_samples = (DstFP**)alloca(sizeof(DstFP*) * input_channels);
      auto out_samples = (DstFP**)alloca(sizeof(DstFP*) * output_channels);

      // Copy & convert input channels
      for (int c = 0; c < input_channels; ++c)
      {
        in_samples[c] = dsp_buffer_input.data() + c * n;
        std::copy_n(in[c], n, in_samples[c]);
      }

      for (int c = 0; c < output_channels; ++c)
      {
        out_samples[c] = dsp_buffer_output.data() + c * n;
      }

      implementation.effect(in_samples, out_samples, n);

      // Copy & convert output channels
      for (int c = 0; c < output_channels; ++c)
      {
        std::copy_n(out_samples[c], n, out[c]);
      }
    }
    else
    {
      // Pass the buffers directly
      implementation.effect(in.data(), out.data(), n);
    }
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation,
      avnd::span<FP*> in,
      avnd::span<FP*> out,
      int32_t n)
  {
    // Note: here we have a redundant check. This is to make sure that we always check the case
    // where we won't have to do a conversion first.
    if constexpr (avnd::polyphonic_arg_audio_effect<FP, T>)
      process_arg<FP, FP>(implementation, in, out, n);
    else if constexpr (avnd::polyphonic_arg_audio_effect<float, T>)
      process_arg<FP, float>(implementation, in, out, n);
    else if constexpr (avnd::polyphonic_arg_audio_effect<double, T>)
      process_arg<FP, double>(implementation, in, out, n);
    else
      AVND_STATIC_TODO(FP)
  }
};

}
