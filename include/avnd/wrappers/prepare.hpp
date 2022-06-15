#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/function_reflection.hpp>
#include <avnd/concepts/audio_processor.hpp>
#include <avnd/wrappers/effect_container.hpp>

namespace avnd
{
struct process_setup
{
  int input_channels{};
  int output_channels{};

  // also known as buffer size
  int frames_per_buffer{};

  // sample rate
  double rate{};

  // Unique instance for the whole "model" object.
  // e.g. a plug-in instantiated twice in the same DAW will get two
  // different instance numbers
  uint64_t instance{};
};

template <typename T>
void prepare(avnd::effect_container<T>& implementation, process_setup setup)
{
  if constexpr (avnd::can_prepare<T>)
  {
    using prepare_type = avnd::first_argument<&T::prepare>;
    prepare_type t;

    // C++20:
    // using "requires" to check easily which members are available
    // in a structure
    if_possible(t.input_channels = setup.input_channels);
    if_possible(t.output_channels = setup.output_channels);
    if_possible(t.channels = setup.output_channels);
    if_possible(t.frames = setup.frames_per_buffer);
    if_possible(t.rate = setup.rate);
    if_possible(t.instance = setup.instance);

    // Coroutines get used here.
    int k = 0;
    for (auto& eff : implementation.effects())
    {
      // Individual instance for the polyphonic object case
      if_possible(t.subinstance = k);
      k++;
      eff.prepare(t);
    }
  }
}

template <typename T>
void prepare(T& implementation, process_setup setup)
{
  if constexpr (avnd::can_prepare<T>)
  {
    using prepare_type = avnd::first_argument<&T::prepare>;
    prepare_type t;

    // C++20:
    // using "requires" to check easily which members are available
    // in a structure
    if_possible(t.input_channels = setup.input_channels);
    if_possible(t.output_channels = setup.output_channels);
    if_possible(t.channels = setup.output_channels);
    if_possible(t.frames = setup.frames_per_buffer);
    if_possible(t.rate = setup.rate);
    if_possible(t.instance = setup.instance);

    implementation.prepare(t);
  }
}

}
