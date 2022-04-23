#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/all.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/port.hpp>
#include <ossia/dataflow/nodes/media.hpp>
#include <ossia/audio/fft.hpp>
#include <boost/mp11.hpp>

namespace oscr
{
template <typename Field>
using spectrum_fft_type
    = ossia::fft;

template <typename T>
struct spectrum_split_channel_input_storage
{
    void init(avnd::effect_container<T>& t, int buffer_size) { }
};

template <typename T>
struct spectrum_complex_channel_input_storage
{
    void init(avnd::effect_container<T>& t, int buffer_size) { }
};

// Field:
// struct { T* amplitude; T* phase; } spectrum;
template <typename T>
requires(avnd::spectrum_split_channel_input_introspection<T>::size > 0)
struct spectrum_split_channel_input_storage<T>
{
  using sc_in = avnd::spectrum_split_channel_input_introspection<T>;

  using fft_tuple = avnd::filter_and_apply<
    spectrum_fft_type,
    avnd::spectrum_split_channel_input_introspection,
    T>;

  // std::tuple< ossia::fft, ossia::fft >
  [[no_unique_address]] fft_tuple ffts;

  void init(avnd::effect_container<T>& t, int buffer_size)
  {
    if constexpr (sc_in::size > 0)
    {
      auto init_raw_in = [&]<auto Idx, typename M>(M& port, avnd::predicate_index<Idx>)
      {
        // Get the matching fft in our storage
        ossia::fft& fft = get<Idx>(this->ffts);

        // Reserve space for the current buffer size
        fft.reset(buffer_size);

        port.spectrum.amplitude = nullptr;
        port.spectrum.phase = nullptr;
      };
      sc_in::for_all_n(avnd::get_inputs(t), init_raw_in);
    }
  }
};

template <typename T>
requires(avnd::spectrum_complex_channel_input_introspection<T>::size > 0)
struct spectrum_complex_channel_input_storage<T>
{
    using sc_in = avnd::spectrum_complex_channel_input_introspection<T>;

    using fft_tuple = avnd::filter_and_apply<
      spectrum_fft_type,
      avnd::spectrum_complex_channel_input_introspection,
    T>;

    // std::tuple< ossia::fft, ossia::fft >
    [[no_unique_address]] fft_tuple ffts;

    void init(avnd::effect_container<T>& t, int buffer_size)
    {
      if constexpr (sc_in::size > 0)
      {
        auto init_raw_in = [&]<auto Idx, typename M>(M& port, avnd::predicate_index<Idx>)
        {
          // Get the matching fft in our storage
          ossia::fft& fft = get<Idx>(this->ffts);

          // Reserve space for the current buffer size
          fft.reset(buffer_size);

          port.spectrum = nullptr;
        };
        sc_in::for_all_n(avnd::get_inputs(t), init_raw_in);
      }
    }
};


template <typename T>
struct spectrum_storage
{
  [[no_unique_address]]
  spectrum_split_channel_input_storage<T> split_channel;
  [[no_unique_address]]
  spectrum_complex_channel_input_storage<T> complex_channel;

  void init(avnd::effect_container<T>& t, int buffer_size)
  {
    split_channel.init(t, buffer_size);
    complex_channel.init(t, buffer_size);
  }
};

}
