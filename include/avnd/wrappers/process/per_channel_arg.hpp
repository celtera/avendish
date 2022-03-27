#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/process/base.hpp>

namespace avnd
{

/**
 * Mono processors with e.g. void operator()(float* in, float* out,...);
 */
template <typename T>
requires(
    avnd::mono_per_channel_arg_processor<
        double,
        T> || avnd::mono_per_channel_arg_processor<float, T>) struct process_adapter<T>
{
  void allocate_buffers(process_setup setup, auto&& f)
  {
    // No buffer to allocates here
  }

  template <typename FP>
  void process_channel(FP* in, FP* out, T& fx, auto& ins, auto& outs, auto&& tick)
  {
    if_possible(fx(in, out, tick)) else if_possible(fx(in, out, ins, tick)) else if_possible(fx(in, out, outs, tick)) else if_possible(
        fx(in,
           out,
           ins,
           outs,
           tick)) else static_assert(std::is_void_v<FP>, "Cannot call processor");
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation,
      avnd::span<FP*> in,
      avnd::span<FP*> out,
      int32_t n)
  {
    const int input_channels = in.size();
    const int output_channels = out.size();
    assert(input_channels == output_channels);
    const int channels = input_channels;

    // Write the output channels
    // C++20: we're using our coroutine here !
    auto effects_range = implementation.full_state();
    auto effects_it = effects_range.begin();
    for (int c = 0; c < channels && effects_it != effects_range.end(); ++c, ++effects_it)
    {
      auto& [impl, ins, outs] = *effects_it;

      if constexpr (requires { sizeof(current_tick(implementation)); })
      {
        process_channel(in[c], out[c], impl, ins, outs, current_tick(implementation));
      }
      else
      {
        process_channel(in[c], out[c], impl, ins, outs, n);
      }
    }
  }
};
}
#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/process/base.hpp>

namespace avnd
{

}
