#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/wrappers/concepts.hpp>
#include <avnd/wrappers/input_introspection.hpp>
#include <avnd/wrappers/output_introspection.hpp>

namespace avnd
{
static const constexpr int undefined_channels = -1;

// Channels set explicitely by the plug-in author
template <typename T>
concept explicit_input_channels
    = (T::input_channels > 0);
template <typename T>
concept explicit_output_channels
    = (T::output_channels > 0);

template <typename T>
concept explicit_input_channels_func
    = (T::input_channels() > 0);
template <typename T>
concept explicit_output_channels_func
    = (T::output_channels() > 0);

template <typename T>
concept explicit_channels = (T::channels > 0);
template <typename T>
concept explicit_channels_func = (T::channels() > 0);

// Channels detected implicitely through the number of audio channels
template <typename T>
concept implicit_io_channels
    = (audio_channel_input_introspection<T>::size > 0)
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

}
