#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/process/base.hpp>

namespace avnd
{

template <typename T>
requires single_audio_bus_poly_port_processor<T>
struct process_adapter<T> : audio_buffer_storage<T>
{
  using i_info = avnd::audio_bus_input_introspection<T>;
  using o_info = avnd::audio_bus_output_introspection<T>;

  template <typename SrcFP, typename DstFP>
  void process_port(
      avnd::effect_container<T>& implementation, avnd::span<SrcFP*> in,
      avnd::span<SrcFP*> out, auto& tick)
  {
    auto& ins = implementation.inputs();
    auto& outs = implementation.outputs();

    const int input_channels = in.size();
    const int output_channels = out.size();
    const auto n = get_frames(tick);

    // Here we get the first audio port declared
    auto& in_port = i_info::template get<0>(ins);
    auto& out_port = o_info::template get<0>(outs);

    // Inputs may be double** or const double**
    using input_fp_type = std::remove_reference_t<decltype(**in_port.samples)>;

    if constexpr(needs_storage<SrcFP, T>::value)
    {
      // Fetch the required temporary storage
      using needed_type = typename needs_storage<SrcFP, T>::needed_storage_t;
      auto& dsp_buffer_input = audio_buffer_storage<T>::input_buffer_for(needed_type{});
      auto& dsp_buffer_output
          = audio_buffer_storage<T>::output_buffer_for(needed_type{});

      // Convert inputs and outputs to double
      in_port.samples = (input_fp_type**)alloca(sizeof(DstFP*) * input_channels);
      out_port.samples = (DstFP**)alloca(sizeof(DstFP*) * output_channels);

      if_possible(in_port.channels = input_channels);
      if_possible(out_port.channels = output_channels);

      // Copy & convert input channels
      for(int c = 0; c < input_channels; ++c)
      {
        auto in_ptr = dsp_buffer_input.data() + c * n;
        std::copy_n(in[c], n, in_ptr);
        in_port.samples[c] = const_cast<input_fp_type*>(in_ptr);
      }

      for(int c = 0; c < output_channels; ++c)
      {
        out_port.samples[c] = dsp_buffer_output.data() + c * n;
      }

      invoke_effect(implementation, get_tick_or_frames(implementation, tick));

      // Copy & convert output channels
      for(int c = 0; c < output_channels; ++c)
      {
        std::copy_n(out_port.samples[c], n, out[c]);
      }
    }
    else
    {
      // Pass the buffers directly
      in_port.samples = const_cast<input_fp_type**>(in.data());
      if_possible(in_port.channels = input_channels);

      out_port.samples = out.data();
      if_possible(out_port.channels = output_channels);

      invoke_effect(implementation, get_tick_or_frames(implementation, tick));

      in_port.samples = nullptr;
      out_port.samples = nullptr;
    }
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation, avnd::span<FP*> in, avnd::span<FP*> out,
      const auto& tick)
  {
    // Note: here we have a redundant check. This is to make sure that we always check the case
    // where we won't have to do a conversion first.
    if constexpr(avnd::polyphonic_single_port_audio_effect<FP, T>)
      process_port<FP, FP>(implementation, in, out, tick);
    else if constexpr(avnd::polyphonic_single_port_audio_effect<float, T>)
      process_port<FP, float>(implementation, in, out, tick);
    else if constexpr(avnd::polyphonic_single_port_audio_effect<double, T>)
      process_port<FP, double>(implementation, in, out, tick);
    else
      AVND_STATIC_TODO(FP)
  }
};

template <typename T>
requires single_audio_input_poly_port_processor<T>
struct process_adapter<T> : audio_buffer_storage<T>
{
  using i_info = avnd::audio_bus_input_introspection<T>;

  template <typename SrcFP, typename DstFP>
  void process_port(
      avnd::effect_container<T>& implementation, avnd::span<SrcFP*> in,
      avnd::span<SrcFP*> out, auto& tick)
  {
    auto& ins = implementation.inputs();

    const int input_channels = in.size();
    const auto n = get_frames(tick);

    // Here we get the first audio port declared
    auto& in_port = i_info::template get<0>(ins);

    // Inputs may be double** or const double**
    using input_fp_type = std::remove_reference_t<decltype(**in_port.samples)>;

    if constexpr(needs_storage<SrcFP, T>::value)
    {
      // Fetch the required temporary storage
      using needed_type = typename needs_storage<SrcFP, T>::needed_storage_t;
      auto& dsp_buffer_input = audio_buffer_storage<T>::input_buffer_for(needed_type{});
      auto& dsp_buffer_output
          = audio_buffer_storage<T>::output_buffer_for(needed_type{});

      // Convert inputs and outputs to double
      in_port.samples = (input_fp_type**)alloca(sizeof(DstFP*) * input_channels);

      if_possible(in_port.channels = input_channels);

      // Copy & convert input channels
      for(int c = 0; c < input_channels; ++c)
      {
        auto in_ptr = dsp_buffer_input.data() + c * n;
        std::copy_n(in[c], n, in_ptr);
        in_port.samples[c] = const_cast<input_fp_type*>(in_ptr);
      }

      invoke_effect(implementation, get_tick_or_frames(implementation, tick));

    }
    else
    {
      // Pass the buffers directly
      in_port.samples = const_cast<input_fp_type**>(in.data());
      if_possible(in_port.channels = input_channels);

      invoke_effect(implementation, get_tick_or_frames(implementation, tick));

      in_port.samples = nullptr;
    }
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation, avnd::span<FP*> in, avnd::span<FP*> out,
      const auto& tick)
  {
    // Note: here we have a redundant check. This is to make sure that we always check the case
    // where we won't have to do a conversion first.
    if constexpr(avnd::polyphonic_single_input_audio_effect<FP, T>)
      process_port<FP, FP>(implementation, in, out, tick);
    else if constexpr(avnd::polyphonic_single_input_audio_effect<float, T>)
      process_port<FP, float>(implementation, in, out, tick);
    else if constexpr(avnd::polyphonic_single_input_audio_effect<double, T>)
      process_port<FP, double>(implementation, in, out, tick);
    else
      AVND_STATIC_TODO(FP)
  }
};


/**
 * Handles case where inputs / outputs are e.g. float** ports with fixed channels being set.
 */
template <typename T>
requires polyphonic_audio_processor<T> &&(
    (poly_array_port_based<float, T>) || (poly_array_port_based<double, T>))
    && (!single_audio_bus_poly_port_processor<T>)
    && (!single_audio_input_poly_port_processor<T>)
struct process_adapter<T>
    : audio_buffer_storage<T>
{
  using i_info = avnd::audio_bus_input_introspection<T>;
  using o_info = avnd::audio_bus_output_introspection<T>;

  template <typename Info, bool Input, typename Ports>
  void initialize_busses(Ports& ports, auto buffers)
  {
    int k = 0;
    Info::for_all(ports, [&](auto& bus) {
      using sample_type = std::decay_t<decltype(bus.samples[0][0])>;
      const int channels = avnd::get_channels(bus);

      if(k + channels <= buffers.size())
      {
        auto buffer = buffers.data() + k;
        bus.samples = const_cast<decltype(bus.samples)>(buffer);
      }
      else
      {
        auto& b = this->zero_storage_for(sample_type{});
        if constexpr(Input)
        {
          auto buffer = b.zero_pointers_in.data();
          bus.samples = const_cast<decltype(bus.samples)>(buffer);
        }
        else
        {
          auto buffer = b.zero_pointers_out.data();
          bus.samples = const_cast<decltype(bus.samples)>(buffer);
        }
      }
      k += channels;
      // FIXME for variable channels, we have to set them beforehand !!
    });
  }

  template <typename Ports>
  void copy_outputs(Ports& ports, auto buffers, int n)
  {
    int k = 0;
    o_info::for_all(ports, [&](auto& bus) {
      using sample_type = std::decay_t<decltype(bus.samples[0][0])>;
      const int channels = avnd::get_channels(bus);
      if(k + channels <= buffers.size())
      {
        for(int c = 0; c < channels; c++)
          std::copy_n(bus.samples[c], n, buffers[k + c]);
      }
      k += channels;
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
        auto i_conv = (DstFP**)alloca(sizeof(DstFP*) * input_channels);
        for(int c = 0; c < input_channels; ++c)
        {
          i_conv[c] = dsp_buffer_input.data() + c * n;
          std::copy_n(in[c], n, i_conv[c]);
        }

        initialize_busses<i_info, true>(
            implementation.inputs(), avnd::span<DstFP*>(i_conv, input_channels));
      }

      // Same process for the outputs
      if constexpr(o_info::size > 0)
      {
        auto& dsp_buffer_output
            = audio_buffer_storage<T>::output_buffer_for(needed_type{});

        auto o_conv = (DstFP**)alloca(sizeof(DstFP*) * output_channels);
        for(int c = 0; c < output_channels; ++c)
        {
          o_conv[c] = dsp_buffer_output.data() + c * n;
        }

        initialize_busses<o_info, false>(
            implementation.outputs(), avnd::span<DstFP*>(o_conv, output_channels));
      }
      // Invoke the effect
      invoke_effect(implementation, get_tick_or_frames(implementation, tick));

      // Copy & convert back output channels
      copy_outputs(implementation.outputs(), out, n);
    }
    else
    {
      initialize_busses<i_info, true>(implementation.inputs(), in);
      initialize_busses<o_info, false>(implementation.outputs(), out);

      invoke_effect(implementation, get_tick_or_frames(implementation, tick));

      i_info::for_all(
          implementation.inputs(), [&](auto& bus) { bus.samples = nullptr; });
      o_info::for_all(
          implementation.outputs(), [&](auto& bus) { bus.samples = nullptr; });
    }
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation, avnd::span<FP*> in, avnd::span<FP*> out,
      const auto& tick)
  {
    // Note: here we have a redundant check. This is to make sure that we always check the case
    // where we won't have to do a conversion first.
    if constexpr(avnd::polyphonic_port_audio_effect<FP, T>)
      process_port<FP, FP>(implementation, in, out, tick);
    else if constexpr(avnd::polyphonic_port_audio_effect<float, T>)
      process_port<FP, float>(implementation, in, out, tick);
    else if constexpr(avnd::polyphonic_port_audio_effect<double, T>)
      process_port<FP, double>(implementation, in, out, tick);
    else
      AVND_STATIC_TODO(FP)
  }
};

}
