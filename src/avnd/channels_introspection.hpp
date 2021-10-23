#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/channels.hpp>

namespace avnd
{

template <typename T>
struct channels_introspection
{
  static constexpr const auto input_channels = undefined_channels;
  static constexpr const auto output_channels = undefined_channels;
};

template <explicit_io_channels T>
struct channels_introspection<T>
{
  static constexpr const auto input_channels = T::input_channels;
  static constexpr const auto output_channels = T::output_channels;
};

template <explicit_channels T>
struct channels_introspection<T>
{
  static constexpr const auto input_channels = T::channels;
  static constexpr const auto output_channels = T::channels;
};

template <implicit_io_channels T>
struct channels_introspection<T>
{
  static constexpr const auto input_channels
      = audio_channel_input_introspection<T>::size;
  static constexpr const auto output_channels
      = audio_channel_output_introspection<T>::size;
};

template <implicit_io_busses T>
struct channels_introspection<T>
{
  static constexpr const auto input_channels
      = count_input_channels_in_busses<T>();
  static constexpr const auto output_channels
      = count_output_channels_in_busses<T>();
};

template <typename T>
static constexpr const int
input_channels(int if_undefined = undefined_channels)
{
  if constexpr (
      channels_introspection<T>::input_channels == undefined_channels)
    return if_undefined;
  return channels_introspection<T>::input_channels;
}

template <typename T>
static constexpr const int
output_channels(int if_undefined = undefined_channels)
{
  if constexpr (
      channels_introspection<T>::output_channels == undefined_channels)
    return if_undefined;
  return channels_introspection<T>::output_channels;
}

}
