#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/process/base.hpp>

namespace avnd
{
// One mono block processor per negotiated channel, with independent state.
template <typename T>
  requires(
      avnd::mono_per_channel_port_processor<double, T>
      || avnd::mono_per_channel_port_processor<float, T>)
struct process_adapter<T> : audio_buffer_storage<T>
{
  using i_info = avnd::audio_channel_input_introspection<T>;
  using o_info = avnd::audio_channel_output_introspection<T>;

  template <std::floating_point FP>
  void allocate_buffers(process_setup setup, FP)
  {
    if constexpr(needs_storage<FP, T>::value)
    {
      using sample_type = typename needs_storage<FP, T>::needed_storage_t;
      // Conversion scratch is reused sequentially, never copied between instances.
      this->input_buffer_for(sample_type{}).resize(setup.frames_per_buffer);
      this->output_buffer_for(sample_type{}).resize(setup.frames_per_buffer);
    }
  }

  template <typename FP>
  void process_channel(FP* input, FP* output, auto state, const auto& tick)
  {
    i_info::for_all(state.inputs, [&](auto& port) { port.channel = input; });
    o_info::for_all(state.outputs, [&](auto& port) { port.channel = output; });
    auto& fx = state.effect;
    if constexpr(requires { fx(state.inputs, state.outputs, tick); })
      fx(state.inputs, state.outputs, tick);
    else if constexpr(requires { fx(state.inputs, tick); })
      fx(state.inputs, tick);
    else if constexpr(requires { fx(state.outputs, tick); })
      fx(state.outputs, tick);
    else if constexpr(requires { fx(tick); })
      fx(tick);
    else if constexpr(requires { fx(state.inputs, state.outputs); })
      fx(state.inputs, state.outputs);
    else if constexpr(requires { fx(state.inputs); })
      fx(state.inputs);
    else if constexpr(requires { fx(state.outputs); })
      fx(state.outputs);
    else if constexpr(requires { fx(); })
      fx();
    else
      static_assert(std::is_void_v<T>, "Cannot call mono block processor");
    i_info::for_all(state.inputs, [](auto& port) { port.channel = nullptr; });
    o_info::for_all(state.outputs, [](auto& port) { port.channel = nullptr; });
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation, avnd::span<FP*> in,
      avnd::span<FP*> out, const auto& tick, auto&&... params)
  {
    assert(in.size() == out.size());
    process_smooth(implementation, params...);
    const auto frames = get_frames(tick);
    const auto local_tick = get_tick_or_frames(implementation, tick);
    auto states = implementation.full_state();
    auto it = states.begin();
    for(std::size_t channel = 0; channel < in.size() && it != states.end(); ++channel, ++it)
    {
      if constexpr(needs_storage<FP, T>::value)
      {
        using sample_type = typename needs_storage<FP, T>::needed_storage_t;
        auto* input = this->input_buffer_for(sample_type{}).data();
        auto* output = this->output_buffer_for(sample_type{}).data();
        std::copy_n(in[channel], frames, input);
        process_channel(input, output, *it, local_tick);
        std::copy_n(output, frames, out[channel]);
      }
      else
      {
        process_channel(in[channel], out[channel], *it, local_tick);
      }
    }
  }
};


/**
 * Handles case where inputs / outputs are e.g. float** ports with fixed channels being set.
 */
template <typename T>
  requires(
      avnd::poly_per_channel_port_processor<double, T>
      || avnd::poly_per_channel_port_processor<float, T>)
struct process_adapter<T> : audio_buffer_storage<T>
{
  using i_info = avnd::audio_channel_input_introspection<T>;
  using o_info = avnd::audio_channel_output_introspection<T>;

  template <typename Info, bool Input, typename Ports>
  void initialize_busses(Ports& ports, auto buffers)
  {
    int k = 0;
    Info::for_all(ports, [&](auto& bus) {
      if(k + 1 <= buffers.size())
      {
        bus.channel = const_cast<decltype(bus.channel)>(buffers[k]);
      }
      else
      {
#if AVND_ENABLE_SAFE_BUFFER_STORAGE
        using sample_type = std::decay_t<decltype(bus.channel[0])>;
        auto& b = this->zero_storage_for(sample_type{});
        if constexpr(Input)
        {
          bus.channel = b.zeros_in.data();
        }
        else
        {
          bus.channel = b.zeros_out.data();
        }
#else
        bus.channel = nullptr;
#endif
      }
      k++;
      // FIXME for variable channels, we have to set them beforehand !!
    });
  }

  template <typename Ports, typename SrcFP>
  void copy_outputs(Ports& ports, avnd::span<SrcFP*> buffers, int n)
  {
    int k = 0;
    o_info::for_all(ports, [&](auto& bus) {
      if(k + 1 <= buffers.size())
      {
        std::copy_n(bus.channel, n, buffers[k]);
      }
      k++;
    });
  }
  template <typename SrcFP, typename DstFP>
  void process_port(
      avnd::effect_container<T>& implementation, avnd::span<SrcFP*> in,
      avnd::span<SrcFP*> out, const auto& tick)
  {
    auto& ins = implementation.inputs();
    auto& outs = implementation.outputs();

    const int input_channels = in.size();
    const int output_channels = out.size();
    const auto n = get_frames(tick);

    if constexpr(needs_storage<SrcFP, T>::value)
    {
      // In this case we need to convert, e.g. from float to double or conversely
      using needed_type = typename needs_storage<SrcFP, T>::needed_storage_t;

      // If there are inputs:
      if constexpr(i_info::size > 0)
      {
        // Fetch the required temporary storage
        auto& dsp_buffer_input
            = audio_buffer_storage<T>::input_buffer_for(needed_type{});

        // Convert inputs to the right FP type, init outputs
        auto i_conv = (DstFP**)alloca(sizeof(DstFP*) * (1 + input_channels));
        for(int c = 0; c < input_channels; ++c)
        {
          i_conv[c] = dsp_buffer_input.data() + c * n;
          std::copy_n(in[c], n, i_conv[c]);
        }

        initialize_busses<i_info, true>(ins, avnd::span<DstFP*>(i_conv, input_channels));
      }

      // Same process for the outputs
      if constexpr(o_info::size > 0)
      {
        auto& dsp_buffer_output
            = audio_buffer_storage<T>::output_buffer_for(needed_type{});

        auto o_conv = (DstFP**)alloca(sizeof(DstFP*) * (1 + output_channels));
        for(int c = 0; c < output_channels; ++c)
        {
          o_conv[c] = dsp_buffer_output.data() + c * n;
        }

        initialize_busses<o_info, false>(
            outs, avnd::span<DstFP*>(o_conv, output_channels));
      }
      // Invoke the effect
      invoke_effect(implementation, get_tick_or_frames(implementation, tick));

      // Copy & convert back output channels
      copy_outputs(outs, out, n);
    }
    else
    {
      // Simple case: effects writes directly on the buffers of the correct type
      initialize_busses<i_info, true>(implementation.inputs(), in);
      initialize_busses<o_info, false>(implementation.outputs(), out);

      invoke_effect(implementation, get_tick_or_frames(implementation, tick));

      i_info::for_all(ins, [&](auto& bus) { bus.channel = nullptr; });
      o_info::for_all(outs, [&](auto& bus) { bus.channel = nullptr; });
    }
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation, avnd::span<FP*> in, avnd::span<FP*> out,
      const auto& tick, auto&&... params)
  {
    // Process the various parameters
    process_smooth(implementation, params...);

    // Note: here we have a redundant check. This is to make sure that we always check the case
    // where we won't have to do a conversion first.
    if constexpr(avnd::monophonic_port_audio_effect<FP, T>)
      process_port<FP, FP>(implementation, in, out, tick);
    else if constexpr(avnd::monophonic_port_audio_effect<float, T>)
      process_port<FP, float>(implementation, in, out, tick);
    else if constexpr(avnd::monophonic_port_audio_effect<double, T>)
      process_port<FP, double>(implementation, in, out, tick);
    else
      AVND_STATIC_TODO(FP)
  }
};

}
