#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/concepts.hpp>
#include <avnd/concepts/audio_processor.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/common/function_reflection.hpp>

namespace avnd
{
struct process_setup
{
  int input_channels{};
  int output_channels{};
  int frames_per_buffer{};
  double rate{};
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

    // Coroutines get used here.
    for (auto& eff : implementation.effects())
      eff.prepare(t);
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

    implementation.prepare(t);
  }
}

}
