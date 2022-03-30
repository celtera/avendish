#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/all.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>

namespace avnd
{

// Channels set explicitely by the plug-in author
template <typename T>
concept explicit_input_channels = (T::input_channels > 0);
template <typename T>
concept explicit_output_channels = (T::output_channels > 0);

template <typename T>
concept explicit_input_channels_func = (T::input_channels() > 0);
template <typename T>
concept explicit_output_channels_func = (T::output_channels() > 0);

template <typename T>
concept explicit_channels = (T::channels > 0);
template <typename T>
concept explicit_channels_func = (T::channels() > 0);

// Channels detected implicitely through the number of audio channels
template <typename T>
concept implicit_io_channels = (audio_channel_input_introspection<T>::size > 0)
                               || (audio_channel_output_introspection<T>::size > 0);

template <typename T>
consteval int channels_in_bus() noexcept
{
  if constexpr (requires { T::channels(); })
    return T::channels();
  return 0;
}

template <typename T>
consteval int count_input_channels_in_busses() noexcept
{
  using introspect_t = audio_bus_input_introspection<T>;
  if constexpr (introspect_t::size > 0)
  {
    // C++20 : template lambda
    int k = 0;
    introspect_t::for_all([&k]<typename FieldT>(FieldT&&)
                          { k += channels_in_bus<typename FieldT::type>(); });

    // This should just be "return k" but GCC had trouble detecting that
    // T::channels() should not have compiled
    return k > 0 ? k : -1;
  }
  return 0;
}

template <typename T>
consteval int count_output_channels_in_busses() noexcept
{
  using introspect_t = audio_bus_output_introspection<T>;
  if constexpr (introspect_t::size > 0)
  {
    int k = 0;
    introspect_t::for_all([&k]<typename FieldT>(FieldT&&)
                          { k += channels_in_bus<typename FieldT::type>(); });

    // This should just be "return k" but GCC had trouble detecting that
    // T::channels() should not have compiled
    return k > 0 ? k : -1;
  }
  return 0;
}

template <typename T>
concept implicit_io_busses = (audio_bus_input_introspection<T>::size > 0
                              || audio_bus_output_introspection<T>::size > 0)
                             && requires
{
  count_input_channels_in_busses<T>();
  count_output_channels_in_busses<T>();
};

/// Input channels introspection
template <typename T>
struct input_channels_introspection
{
  static constexpr const auto input_channels = undefined_channels;
};

template <explicit_input_channels T>
struct input_channels_introspection<T>
{
  static constexpr const auto input_channels = T::input_channels;
};

template <explicit_channels T>
struct input_channels_introspection<T>
{
  static constexpr const auto input_channels = T::channels;
};

template <explicit_input_channels_func T>
struct input_channels_introspection<T>
{
  static constexpr const auto input_channels = T::input_channels();
};

template <explicit_channels_func T>
struct input_channels_introspection<T>
{
  static constexpr const auto input_channels = T::channels();
};

template <implicit_io_channels T>
struct input_channels_introspection<T>
{
  static constexpr const auto input_channels
      = audio_channel_input_introspection<T>::size;
};

template <implicit_io_busses T>
struct input_channels_introspection<T>
{
  static constexpr const auto input_channels = count_input_channels_in_busses<T>();
};

/// Output channels introspection
template <typename T>
struct output_channels_introspection
{
  static constexpr const auto output_channels = undefined_channels;
};

template <explicit_output_channels T>
struct output_channels_introspection<T>
{
  static constexpr const auto output_channels = T::output_channels;
};

template <explicit_channels T>
struct output_channels_introspection<T>
{
  static constexpr const auto output_channels = T::channels;
};

template <explicit_output_channels_func T>
struct output_channels_introspection<T>
{
  static constexpr const auto output_channels = T::output_channels();
};

template <explicit_channels_func T>
struct output_channels_introspection<T>
{
  static constexpr const auto output_channels = T::channels();
};

template <implicit_io_channels T>
struct output_channels_introspection<T>
{
  static constexpr const auto output_channels
      = audio_channel_output_introspection<T>::size;
};

template <implicit_io_busses T>
struct output_channels_introspection<T>
{
  static constexpr const auto output_channels = count_output_channels_in_busses<T>();
};

/// Utilities for introspection
template <typename T>
struct channels_introspection
    : input_channels_introspection<T>
    , output_channels_introspection<T>
{
};

template <typename T>
static constexpr int input_channels(int if_undefined = undefined_channels)
{
  if constexpr (input_channels_introspection<T>::input_channels == undefined_channels)
    return if_undefined;
  return input_channels_introspection<T>::input_channels;
}

template <typename T>
static constexpr int output_channels(int if_undefined = undefined_channels)
{
  if constexpr (output_channels_introspection<T>::output_channels == undefined_channels)
    return if_undefined;
  return output_channels_introspection<T>::output_channels;
}

/// Bus introspection
template <typename T>
struct bus_introspection
{
  static constexpr const auto input_busses = 0;
  static constexpr const auto output_busses = 0;
};

// float operator()(float in);
template <typename T>
requires mono_per_sample_arg_processor<float, T> || mono_per_sample_arg_processor<
    double,
    T>
struct bus_introspection<T>
{
  static constexpr const auto input_busses = 1;
  static constexpr const auto output_busses = 1;
};

// void operator()(float* in, float* out);
template <typename T>
requires monophonic_arg_audio_effect<float, T> || monophonic_arg_audio_effect<double, T>
struct bus_introspection<T>
{
  static constexpr const auto input_busses = 1;
  static constexpr const auto output_busses = 1;
};

// void operator()(float** in, float** out);
template <typename T>
requires polyphonic_arg_audio_effect<float, T> || polyphonic_arg_audio_effect<double, T>
struct bus_introspection<T>
{
  static constexpr const auto input_busses = 1;
  static constexpr const auto output_busses = 1;
};

template <typename T>
requires(sample_input_port_count<float, T> != 0)
    || (sample_output_port_count<float, T> != 0)
    || (sample_input_port_count<double, T> != 0)
    || (sample_output_port_count<double, T> != 0) struct bus_introspection<T>
{
  // TODO group them as busses instead ?
  static constexpr const auto input_busses = sample_input_port_count<float, T> + sample_input_port_count<double, T>;
  static constexpr const auto output_busses = sample_output_port_count<float, T> + sample_output_port_count<double, T>;
};

template <typename T>
requires(mono_sample_array_input_port_count<float, T> != 0)
    || (mono_sample_array_output_port_count<float, T> != 0)
    || (mono_sample_array_input_port_count<double, T> != 0)
    || (mono_sample_array_output_port_count<double, T> != 0) struct bus_introspection<T>
{
  // TODO group them as busses instead ?
  static constexpr const auto input_busses = mono_sample_array_input_port_count<float, T> + mono_sample_array_input_port_count<double, T>;
  static constexpr const auto output_busses = mono_sample_array_output_port_count<float, T> + mono_sample_array_output_port_count<double, T>;
};

template <typename T>
requires(poly_sample_array_input_port_count<float, T> != 0)
    || (poly_sample_array_output_port_count<float, T> != 0)
    || (poly_sample_array_input_port_count<double, T> != 0)
    || (poly_sample_array_output_port_count<double, T> != 0) struct bus_introspection<T>
{
  static constexpr const auto input_busses = audio_bus_input_introspection<T>::size;
  static constexpr const auto output_busses = audio_bus_input_introspection<T>::size;
};

}
