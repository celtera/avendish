#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>

#include <string_view>

namespace examples
{
struct Presets
{
  static consteval auto name() { return "Presets example"; }
  static consteval auto c_name() { return "avnd_presets"; }
  static consteval auto uuid() { return "08176877-ce82-4bee-b885-df42062fb8a2"; }

  // Here we force the presence of two channels.
  // It's possible to force separate inputs and outputs by
  // having input_channels and output_channels variables.
  static constexpr const int channels = 2;

  struct
  {
    // A first control
    struct
    {
      static consteval auto name() { return "Preamp"; }
      float value{0.5};
    } preamp;

    // A second control
    struct
    {
      static consteval auto name() { return "Volume"; }
      float value{1.0};
    } volume;
  } inputs;

  // We define the type of our programs, like in the other cases
  // it ends up being introspected automatically.
  struct program
  {
    std::string_view name;
    decltype(Presets::inputs) parameters;
  };

  // Note: it's an array instead of a function because
  // it's apparently hard to deduce N in array<..., N>, unlike in C arrays.
  static constexpr const program programs[]{
      {.name{"Low gain"}, .parameters{.preamp = {0.3}, .volume = {0.6}}},
      {.name{"Hi gain"}, .parameters{.preamp = {1.0}, .volume = {1.0}}},
  };

  void operator()(double** in, double** out, int frames)
  {
    const double preamp = 100. * inputs.preamp.value;
    const double volume = inputs.volume.value;

    for (int c = 0; c < channels; c++)
      for (int i = 0; i < frames; i++)
        out[c][i] = volume * std::tanh(in[c][i] * preamp);
  }
};
}
