#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{

// struct { float sample; };
template <typename FP, typename T>
concept audio_sample_port = std::same_as<decltype(T::sample), FP>;
template <typename T>
concept generic_audio_sample_port = std::floating_point<decltype(T::sample)>;

template <typename FP, typename T>
using is_audio_sample_port
    = std::conditional_t<audio_sample_port<FP, T>, std::true_type, std::false_type>;

template <typename FP>
struct is_audio_sample_port_q
{
  template <typename T>
  using fn
      = std::conditional_t<audio_sample_port<FP, T>, std::true_type, std::false_type>;
};

// struct { float* channel; };
template <typename FP, typename T>
concept mono_array_sample_port = std::same_as<decltype(T::channel), FP*>
                                 || std::same_as<decltype(T::channel), const FP*>;

template <typename FP, typename T>
using is_mono_array_sample_port
    = std::conditional_t<mono_array_sample_port<FP, T>, std::true_type, std::false_type>;

template <typename FP>
struct is_mono_array_sample_port_q
{
  template <typename T>
  using fn = std::conditional_t<
      mono_array_sample_port<FP, T>, std::true_type, std::false_type>;
};

// struct { float** samples; };
template <typename FP, typename T>
concept poly_array_sample_port = std::same_as<decltype(T::samples), FP**>
                                 || std::same_as<decltype(T::samples), const FP**>;

template <typename FP, typename T>
using is_poly_array_sample_port
    = std::conditional_t<poly_array_sample_port<FP, T>, std::true_type, std::false_type>;

template <typename FP>
struct is_poly_array_sample_port_q
{
  template <typename T>
  using fn = std::conditional_t<
      poly_array_sample_port<FP, T>, std::true_type, std::false_type>;
};

template <typename T>
concept mono_audio_port
    = audio_sample_port<float, T> || audio_sample_port<double, T>
      || mono_array_sample_port<float, T> || mono_array_sample_port<double, T>;

template <typename T>
concept poly_audio_port
    = poly_array_sample_port<float, T> || poly_array_sample_port<double, T>;

template <typename T>
concept fixed_poly_audio_port = poly_audio_port<T> && requires(T t) {
                                                        {
                                                          t.channels()
                                                          } -> avnd::int_ish;
                                                      };

template <typename T>
concept dynamic_poly_audio_port = poly_audio_port<T> && !
fixed_poly_audio_port<T>;

template <typename T>
concept variable_poly_audio_port
    = dynamic_poly_audio_port<T> && requires(T t) { t.request_channels = {}; };

template <typename T>
concept audio_port = mono_audio_port<T> || poly_audio_port<T>;

int get_channels(fixed_poly_audio_port auto& port)
{
  return port.channels();
}
int get_channels(dynamic_poly_audio_port auto& port)
{
  return port.channels;
}

}
