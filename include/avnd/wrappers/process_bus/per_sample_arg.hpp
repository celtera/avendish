#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/process_bus/base.hpp>

namespace avnd
{

/**
 * Mono processors with e.g. float operator()(float in, ...);
 */
template <typename T>
  requires(
      avnd::mono_per_sample_arg_processor<double, T>
      || avnd::mono_per_sample_arg_processor<float, T>)
struct process_bus_adapter<T>
{
  void allocate_buffers(process_setup setup, auto&& f)
  {
    // No buffer to allocates here
  }

  template <typename FP>
  FP process_sample(FP in, T& fx, auto& ins, auto& outs, const auto& tick)
  {
    if constexpr(requires { fx(in, ins, outs, tick); })
      return fx(in, ins, outs, tick);
    else if constexpr(requires { fx(in, ins, tick); })
      return fx(in, ins, tick);
    else if constexpr(requires { fx(in, outs, tick); })
      return fx(in, outs, tick);
    else if constexpr(requires { fx(in, tick); })
      return fx(in, tick);
    else if constexpr(requires { fx(tick); })
      return fx(tick);
    else
      static_assert(std::is_void_v<FP>, "Canno call processor");
  }

  template <typename FP>
  FP process_sample(FP in, T& fx, auto& ins, auto& outs)
  {
    if constexpr(requires { fx(in, ins, outs); })
      return fx(in, ins, outs);
    else if constexpr(requires { fx(in, ins); })
      return fx(in, ins);
    else if constexpr(requires { fx(in, outs); })
      return fx(in, outs);
    else if constexpr(requires { fx(in); })
      return fx(in);
    else if constexpr(requires { fx(); })
      return fx(in);
    else
      static_assert(std::is_void_v<FP>, "Canno call processor");
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation, avnd::span<avnd::span<FP*>> in,
      avnd::span<avnd::span<FP*>> out,
      const auto& tick, auto&&... params)
  {
    const int input_channels = in.size();
    const int output_channels = out.size();
    assert(input_channels == output_channels);
    const int channels = input_channels;
    const auto n = get_frames(tick);

    auto input_buf = (FP*)alloca(channels * sizeof(FP));

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
        auto [impl, ins, outs] = *effects_it;
        static_assert(std::is_reference_v<decltype(impl)>);

        if constexpr(avnd::has_tick<T>)
        {
          out[c][i] = process_sample(
              input_buf[c], impl, ins, outs, get_tick_or_frames(implementation, tick));
        }
        else
        {
          out[c][i] = process_sample(input_buf[c], impl, ins, outs);
        }
      }
    }
  }
};
}
