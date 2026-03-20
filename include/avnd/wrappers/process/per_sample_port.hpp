#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/process/base.hpp>

namespace avnd
{

/**
 * Mono processors with e.g. struct { float sample; } audio_in;
 */
template <typename T>
  requires(
      avnd::mono_per_sample_port_processor<double, T>
      || avnd::mono_per_sample_port_processor<float, T>)
struct process_adapter<T>
{
  void allocate_buffers(process_setup setup, auto&& f)
  {
    // No buffer to allocates here
  }

  // Here we know that we at least have one in and one out
  template <typename FP>
  FP process_0(avnd::effect_container<T>& implementation, FP in, auto ref, auto&& tick)
  {
    auto& [fx, ins, outs] = ref;
    // Copy the input
    avnd::for_each_field_ref(ins, [in]<typename Field>(Field& field) {
      // We know that there is only one in that case so just copy
      if_possible(field.sample = in);
    });

    // Execute
    if_possible(ref.effect(ref.inputs, ref.outputs, tick)) else if_possible(
        ref.effect(tick));

    // Read back the output the input
    FP out;
    avnd::for_each_field_ref(
        outs, [&out]<typename Field>(Field& field) { if_possible(out = field.sample); });
    return out;
  }

  template <typename FP>
  FP process_0(avnd::effect_container<T>& implementation, FP in, auto ref)
  {
    auto& [fx, ins, outs] = ref;
    // Copy the input
    avnd::for_each_field_ref(
        ins, [in]<typename Field>(Field& field) { if_possible(field.sample = in); });

    // Execute
    if_possible(ref.effect(ref.inputs, ref.outputs)) else if_possible(ref.effect());

    // Read back the output
    FP out;
    avnd::for_each_field_ref(
        outs, [&out]<typename Field>(Field& field) { if_possible(out = field.sample); });
    return out;
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation, avnd::span<FP*> in, avnd::span<FP*> out,
      const auto& tick, auto&&... params)
  {
    const int input_channels = in.size();
    const int output_channels = out.size();
    assert(input_channels == output_channels);
    const int channels = input_channels;

    auto input_buf = (FP*)alloca((1 + channels) * sizeof(FP));

    const auto n = get_frames(tick);
    for(int32_t i = 0; i < n; i++)
    {
      // Some hosts like puredata uses the same buffers for input and output.
      // Thus, we have to :
      //  1/ fetch all inputs
      //  2/ apply fx
      //  3/ store all outputs
      // otherwise writing out[0] may overwrite in[1]
      // before we could read it for instance

      // Copy the input channels
      for(int c = 0; c < channels; c++)
      {
        input_buf[c] = in[c][i];
      }

      // Process the various parameters
      process_smooth(implementation, params...);

      // Write the output channels
      // C++20: we're using our coroutine here !
      auto effects_range = implementation.full_state();
      auto effects_it = effects_range.begin();
      for(int c = 0; c < channels && effects_it != effects_range.end();
          ++c, ++effects_it)
      {
        if constexpr(avnd::has_tick<T>)
        {
          out[c][i] = process_0(
              implementation, input_buf[c], *effects_it, current_tick(implementation));
        }
        else
        {
          out[c][i] = process_0(implementation, input_buf[c], *effects_it);
        }
      }
    }
  }
};

/**
 * Handles case where inputs / outputs are multiple one-sample ports
 */
template <typename T>
  requires(
      poly_per_sample_port_processor<float, T>
      || poly_per_sample_port_processor<double, T>)
struct process_adapter<T>
{
  void process_sample(T& fx, auto& ins, auto& outs, auto&& tick)
  {
    if constexpr(requires { fx(ins, outs, tick); })
      return fx(ins, outs, tick);
    else if constexpr(requires { fx(ins, tick); })
      return fx(ins, tick);
    else if constexpr(requires { fx(outs, tick); })
      return fx(outs, tick);
    else if constexpr(requires { fx(tick); })
      return fx(tick);
    else
      static_assert(std::is_void_v<T>, "Canno call processor");
  }

  void process_sample(T& fx, auto& ins, auto& outs)
  {
    if constexpr(requires { fx(ins, outs); })
      return fx(ins, outs);
    else if constexpr(requires { fx(ins); })
      return fx(ins);
    else if constexpr(requires { fx(outs); })
      return fx(outs);
    else if constexpr(requires { fx(); })
      return fx();
    else
      static_assert(std::is_void_v<T>, "Canno call processor");
  }

  void allocate_buffers(process_setup setup, auto&& f)
  {
    // nothing to allocate since we're processing per-sample
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation, avnd::span<FP*> in, avnd::span<FP*> out,
      const auto& tick, auto&&... params)
  {
    auto& fx = implementation.effect;
    auto& ins = implementation.inputs();
    auto& outs = implementation.outputs();

    const auto n = get_frames(tick);
    for(int32_t i = 0; i < n; i++)
    {
      // Copy inputs in the effect
      {
        int k = 0;
        // Here we know that we have a single effect. We copy the sample data directly inside.
        avnd::for_each_field_ref(ins, [&k, in, i]<typename Field>(Field& field) {
          if constexpr(avnd::generic_audio_sample_port<Field>)
          {
            if(k < in.size())
            {
              field.sample = in[k][i];
              ++k;
            }
          }
        });
      }

      // Process the various parameters
      process_smooth(implementation, params...);

      // Process
      if constexpr(avnd::has_tick<T>)
      {
        process_sample(fx, ins, outs, current_tick(implementation));
      }
      else
      {
        process_sample(fx, ins, outs);
      }

      // Copy the outputs
      {
        int k = 0;
        // Here we know that we have a single effect. We copy the sample data directly inside.
        avnd::for_each_field_ref(outs, [&k, out, i]<typename Field>(Field& field) {
          if constexpr(avnd::generic_audio_sample_port<Field>)
          {
            if(k < out.size())
            {
              out[k][i] = field.sample;
              ++k;
            }
          }
        });
      }
    }
  }
};

}
