#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/effect_container.hpp>
#include <avnd/input_introspection.hpp>
#include <avnd/output_introspection.hpp>
#include <avnd/prepare.hpp>
#include <boost/pfr.hpp>
#include <common/function_reflection.hpp>

#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <utility>
#include <vector>

#include <type_traits>

namespace avnd
{
template <typename FP, typename T>
struct needs_storage : std::false_type
{
  using needed_storage_t = void;
};

// If our processor supports doubles, and does not support float
// And our host wants to send float, then we have to allocate a buffer
// of doubles
template <typename T>
requires(
    avnd::double_processor<
        T> && !avnd::float_processor<T>) struct needs_storage<float, T>
    : std::true_type
{
  using needed_storage_t = double;
};

template <typename T>
requires(
    !avnd::double_processor<
        T> && avnd::float_processor<T>) struct needs_storage<double, T>
    : std::true_type
{
  using needed_storage_t = float;
};

template <typename FP, typename T>
using buffer_type = std::conditional_t<
    needs_storage<FP, T>::value,
    std::vector<
        typename needs_storage<FP, T>::needed_storage_t> // TODO aligned alloc
    ,
    dummy>;

template <typename T>
concept has_tick = std::is_default_constructible_v<typename T::tick>;

// Original idea was to pass everything by arguments here.
// Sadly hard to do as soon as there are references as we cannot do it piecewise
template <typename T>
auto current_tick(avnd::effect_container<T>& implementation)
{
  // Nice little C++20 goodie: remove_cvref_t
  // unused in the end using tick_setup_t = std::remove_cvref_t<avnd::second_argument<&T::operator()>>;
  if constexpr (requires { typename T::tick; })
  {
    using tick_t = typename T::tick;
    static_assert(std::is_aggregate_v<tick_t>);

    // TODO setup shjit

    return tick_t{};
  }
}

/**
 * This class is used to adapt between hosts that will send audio as arrays of float** / double** channels
 * to various useful cases
 */
template <typename T>
struct process_adapter;

/**
 * Handles case where inputs / outputs are e.g. float** ports
 */
template <typename T>
requires polyphonic_audio_processor<T> &&(
    poly_array_port_based<
        float,
        T> || poly_array_port_based<double, T>)struct process_adapter<T>
{
  // buffers used in case we need to convert float -> double
  [[no_unique_address]] buffer_type<double, T> m_dsp_buffer_input_f;
  [[no_unique_address]] buffer_type<double, T> m_dsp_buffer_output_f;
  [[no_unique_address]] buffer_type<float, T> m_dsp_buffer_input_d;
  [[no_unique_address]] buffer_type<float, T> m_dsp_buffer_output_d;

  auto& input_buffer_for(float) { return m_dsp_buffer_input_f; }
  auto& input_buffer_for(double) { return m_dsp_buffer_input_d; }
  auto& output_buffer_for(float) { return m_dsp_buffer_output_f; }
  auto& output_buffer_for(double) { return m_dsp_buffer_output_d; }

  template <std::floating_point SrcFP>
  void allocate_buffers(process_setup setup, SrcFP f)
  {
    // If our effect is written with doubles, and we're in a host
    // which requires floats, we allocate buffers to store the converted data
    if constexpr (needs_storage<SrcFP, T>::value)
    {
      using needed_type = typename needs_storage<SrcFP, T>::needed_storage_t;
      input_buffer_for(needed_type{})
          .resize(setup.frames_per_buffer * setup.input_channels);
      output_buffer_for(needed_type{})
          .resize(setup.frames_per_buffer * setup.output_channels);
    }
  }

  template <typename SrcFP, typename DstFP>
  void process_port(
      avnd::effect_container<T>& implementation,
      std::span<SrcFP*> in,
      std::span<SrcFP*> out,
      int32_t n)
  {
    using i_info = avnd::audio_bus_input_introspection<T>;
    using o_info = avnd::audio_bus_output_introspection<T>;
    auto& ins = implementation.inputs();
    auto& outs = implementation.outputs();

    const int input_channels = in.size();
    const int output_channels = out.size();

    // Here we get the first audio port declared
    auto& in_port = boost::pfr::get<i_info::index_map[0]>(ins);
    auto& out_port = boost::pfr::get<o_info::index_map[0]>(outs);

    if constexpr (needs_storage<SrcFP, T>::value)
    {
      // Fetch the required temporary storage
      using needed_type = typename needs_storage<SrcFP, T>::needed_storage_t;
      auto& dsp_buffer_input = input_buffer_for(needed_type{});
      auto& dsp_buffer_output = output_buffer_for(needed_type{});

      // Convert inputs and outputs to double
      in_port.samples = (DstFP**)alloca(sizeof(DstFP*) * input_channels);
      out_port.samples = (DstFP**)alloca(sizeof(DstFP*) * output_channels);
      in_port.channels = input_channels;
      if_possible(out_port.channels = output_channels);

      // Copy & convert input channels
      for (int c = 0; c < input_channels; ++c)
      {
        in_port.samples[c] = dsp_buffer_input.data() + c * n;
        std::copy_n(in[c], n, in_port.samples[c]);
      }

      for (int c = 0; c < output_channels; ++c)
      {
        out_port.samples[c] = dsp_buffer_output.data() + c * n;
      }

      implementation.effect(n);

      // Copy & convert output channels
      for (int c = 0; c < output_channels; ++c)
      {
        std::copy_n(out_port.samples[c], n, out[c]);
      }
    }
    else
    {
      // Pass the buffers directly
      in_port.samples = in.data();
      in_port.channels = input_channels;

      out_port.samples = out.data();
      if_possible(out_port.channels = output_channels);

      implementation.effect(n);

      in_port.samples = nullptr;
      out_port.samples = nullptr;
    }
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation,
      std::span<FP*> in,
      std::span<FP*> out,
      int32_t n)
  {
    // Note: here we have a redundant check. This is to make sure that we always check the case
    // where we won't have to do a conversion first.
    if constexpr (avnd::polyphonic_port_audio_effect<FP, T>)
      process_port<FP, FP>(implementation, in, out, n);
    else if constexpr (avnd::polyphonic_port_audio_effect<float, T>)
      process_port<FP, float>(implementation, in, out, n);
    else if constexpr (avnd::polyphonic_port_audio_effect<double, T>)
      process_port<FP, double>(implementation, in, out, n);
    else
      STATIC_TODO(FP)
  }
};

/**
 * Handles case where inputs / outputs are e.g. operator()(float** ins, float** outs)
 */
template <typename T>
requires polyphonic_audio_processor<T> &&(
    polyphonic_arg_audio_effect<
        double,
        T> || polyphonic_arg_audio_effect<float, T>)struct process_adapter<T>
{
  // buffers used in case we need to convert float -> double
  [[no_unique_address]] buffer_type<double, T> m_dsp_buffer_input_f;
  [[no_unique_address]] buffer_type<double, T> m_dsp_buffer_output_f;
  [[no_unique_address]] buffer_type<float, T> m_dsp_buffer_input_d;
  [[no_unique_address]] buffer_type<float, T> m_dsp_buffer_output_d;

  auto& input_buffer_for(float) { return m_dsp_buffer_input_f; }
  auto& input_buffer_for(double) { return m_dsp_buffer_input_d; }
  auto& output_buffer_for(float) { return m_dsp_buffer_output_f; }
  auto& output_buffer_for(double) { return m_dsp_buffer_output_d; }

  template <std::floating_point SrcFP>
  void allocate_buffers(process_setup setup, SrcFP f)
  {
    // If our effect is written with doubles, and we're in a host
    // which requires floats, we allocate buffers to store the converted data
    if constexpr (needs_storage<SrcFP, T>::value)
    {
      using needed_type = typename needs_storage<SrcFP, T>::needed_storage_t;
      input_buffer_for(needed_type{})
          .resize(setup.frames_per_buffer * setup.input_channels);
      output_buffer_for(needed_type{})
          .resize(setup.frames_per_buffer * setup.output_channels);
    }
  }

  template <typename SrcFP, typename DstFP>
  void process_arg(
      avnd::effect_container<T>& implementation,
      std::span<SrcFP*> in,
      std::span<SrcFP*> out,
      int32_t n)
  {
    const int input_channels = in.size();
    const int output_channels = out.size();
    if constexpr (needs_storage<SrcFP, T>::value)
    {
      // Fetch the required temporary storage
      using needed_type = typename needs_storage<SrcFP, T>::needed_storage_t;
      auto& dsp_buffer_input = input_buffer_for(needed_type{});
      auto& dsp_buffer_output = output_buffer_for(needed_type{});

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
      std::span<FP*> in,
      std::span<FP*> out,
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
      STATIC_TODO(FP)
  }
};

/**
 * Mono processors with e.g. float operator()(float in, ...);
 */
template <typename T>
requires(
    avnd::mono_per_sample_arg_processor<
        double,
        T> || avnd::mono_per_sample_arg_processor<float, T>) struct
    process_adapter<T>
{
  void allocate_buffers(process_setup setup, auto&& f)
  {
    // No buffer to allocates here
  }

  template <typename FP>
  FP process_sample(
      avnd::effect_container<T>& implementation,
      FP in,
      auto& ref,
      auto&& tick)
  {
    return std::apply(
        [&]<typename... Args>(T & fx, Args && ... args)
        { return fx(in, args..., tick); },
        boost::pfr::structure_tie(ref));
  }

  template <typename FP>
  FP process_sample(
      avnd::effect_container<T>& implementation,
      FP in,
      auto& ref)
  {
    return std::apply(
        [&]<typename... Args>(T & fx, Args && ... args)
        { return fx(in, args...); },
        boost::pfr::structure_tie(ref));
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation,
      std::span<FP*> in,
      std::span<FP*> out,
      int32_t n)
  {
    const int input_channels = in.size();
    const int output_channels = out.size();
    assert(input_channels == output_channels);
    const int channels = input_channels;

    auto input_buf = (FP*)alloca(channels * sizeof(FP));

    for (int32_t i = 0; i < n; i++)
    {
      // Some hosts like puredata uses the same buffers for input and output.
      // Thus, we have to :
      //  1/ fetch all inputs
      //  2/ apply fx
      //  3/ store all outputs
      // otherwise writing out[0] may overwrite in[1]
      // before we could read it for instance

      // Copy the input channels
      for (int c = 0; c < channels; c++)
      {
        input_buf[c] = in[c][i];
      }

      // Write the output channels
      // C++20: we're using our coroutine here !
      auto effects_range = implementation.full_state();
      auto effects_it = effects_range.begin();
      for (int c = 0; c < channels; c++)
      {
        assert(effects_it != effects_range.end());

        if constexpr (requires { sizeof(current_tick(implementation)); })
        {
          out[c][i] = process_sample(
              implementation,
              input_buf[c],
              *effects_it,
              current_tick(implementation));
        }
        else
        {
          out[c][i]
              = process_sample(implementation, input_buf[c], *effects_it);
        }

        ++effects_it;
      }
    }
  }
};

/**
 * Mono processors with e.g. struct { float sample; } audio_in;
 */
template <typename T>
requires(
    avnd::mono_per_sample_port_processor<
        double,
        T> || avnd::mono_per_sample_port_processor<float, T>) struct
    process_adapter<T>
{
  void allocate_buffers(process_setup setup, auto&& f)
  {
    // No buffer to allocates here
  }

  // Here we know that we at least have one in and one out
  template <typename FP>
  FP process_0(
      avnd::effect_container<T>& implementation,
      FP in,
      auto& ref,
      auto&& tick)
  {
    auto& [fx, ins, outs] = ref;
    // Copy the input
    boost::pfr::for_each_field(
        ins,
        [in]<typename Field>(Field& field)
        {
          if constexpr (is_audio_sample_port<FP, Field>::value)
          {
            // We know that there is only one in that case so just copy
            field.sample = in;
          }
        });

    // Execute
    fx(ins, outs, tick);

    // Read back the output the input
    FP out;
    boost::pfr::for_each_field(
        outs,
        [&out]<typename Field>(Field& field)
        {
          if constexpr (is_audio_sample_port<FP, Field>::value)
          {
            // We know that there is only one in that case so just copy
            out = field.sample;
          }
        });
    return out;
  }

  template <typename FP>
  FP process_0(avnd::effect_container<T>& implementation, FP in, auto& ref)
  {
    auto& [fx, ins, outs] = ref;
    // Copy the input
    boost::pfr::for_each_field(
        ins,
        [in]<typename Field>(Field& field)
        {
          if constexpr (is_audio_sample_port<FP, Field>::value)
          {
            // We know that there is only one in that case so just copy
            field.sample = in;
          }
        });

    // Execute
    fx(ins, outs);

    // Read back the output the input
    FP out;
    boost::pfr::for_each_field(
        outs,
        [&out]<typename Field>(Field& field)
        {
          if constexpr (is_audio_sample_port<FP, Field>::value)
          {
            // We know that there is only one in that case so just copy
            out = field.sample;
          }
        });
    return out;
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation,
      std::span<FP*> in,
      std::span<FP*> out,
      int32_t n)
  {
    const int input_channels = in.size();
    const int output_channels = out.size();
    assert(input_channels == output_channels);
    const int channels = input_channels;

    auto input_buf = (FP*)alloca(channels * sizeof(FP));

    for (int32_t i = 0; i < n; i++)
    {
      // Some hosts like puredata uses the same buffers for input and output.
      // Thus, we have to :
      //  1/ fetch all inputs
      //  2/ apply fx
      //  3/ store all outputs
      // otherwise writing out[0] may overwrite in[1]
      // before we could read it for instance

      // Copy the input channels
      for (int c = 0; c < channels; c++)
      {
        input_buf[c] = in[c][i];
      }

      // Write the output channels
      // C++20: we're using our coroutine here !
      auto effects_range = implementation.full_state();
      auto effects_it = effects_range.begin();
      for (int c = 0; c < channels; c++)
      {
        assert(effects_it != effects_range.end());

        if constexpr (requires { sizeof(current_tick(implementation)); })
        {
          out[c][i] = process_0(
              implementation,
              input_buf[c],
              *effects_it,
              current_tick(implementation));
        }
        else
        {
          out[c][i] = process_0(implementation, input_buf[c], *effects_it);
        }
        ++effects_it;
      }
    }
  }
};

/**
 * Handles case where inputs / outputs are multiple one-sample ports
 */
template <typename T>
requires(
    poly_per_sample_port_processor<
        float,
        T> || poly_per_sample_port_processor<double, T>) struct
    process_adapter<T>
{
  void allocate_buffers(process_setup setup, auto&& f)
  {
    // nothing to allocate since we're processing per-sample
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation,
      std::span<FP*> in,
      std::span<FP*> out,
      int32_t n)
  {
    // We have N input channels.
    const int input_channels = in.size();
    const int output_channels = out.size();

    // TODO assert that channel count matches !

    auto& fx = implementation.effect;

    for (int32_t i = 0; i < n; i++)
    {
      // Copy inputs in the effect
      {
        int k = 0;
        // Here we know that we have a single effect. We copy the sample data directly inside.
        boost::pfr::for_each_field(
            fx.inputs,
            [&k, in, i]<typename Field>(Field& field)
            {
              if constexpr (avnd::generic_audio_sample_port<Field>)
              {
                if(k < in.size())
                {
                  field.sample = in[k][i];
                  ++k;
                }
              }
            });
      }

      // Process
      fx();

      // Copy the outputs
      {
        int k = 0;
        // Here we know that we have a single effect. We copy the sample data directly inside.
        boost::pfr::for_each_field(
            fx.outputs,
            [&k, out, i]<typename Field>(Field& field)
            {
              if constexpr (avnd::generic_audio_sample_port<Field>)
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
