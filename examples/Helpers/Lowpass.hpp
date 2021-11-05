#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <helpers/meta.hpp>
#include <helpers/audio.hpp>
#include <helpers/controls.hpp>
#include <vector>

namespace examples::helpers
{

/**
 * Same as the "simple" example, but with the helpers library
 */
class Lowpass
{
public:
  $(name, "Lowpass (helpers)")
  $(c_name, "avnd_helpers_lowpass")
  $(uuid, "82bdb9b5-9cf8-440e-8675-c0caf4fc59b9")

  using setup = avnd::setup;
  using tick = avnd::tick;

  struct
  {
    avnd::dynamic_audio_bus<"Input", double> audio;
    avnd::hslider_f32<"Weight", avnd::range{.min = 0., .max = 1., .init = 0.5}> weight;
  } inputs;

  struct
  {
    avnd::dynamic_audio_bus<"Output", double> audio;
  } outputs;

  void prepare(avnd::setup info)
  {
    previous_values.resize(info.input_channels);
  }

  // Do our processing for N samples
  void operator()(avnd::tick t)
  {
    const auto weight = inputs.weight.value;
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;

    const auto chans = p1.channels;

    // Process the input buffer
    for (int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];

      float& prev = this->previous_values[i];

      for (int j = 0; j < t.frames; j++)
      {
        out[j] = weight * in[j] + (1.0 - weight) * prev;
        prev = out[j];
      }
    }
  }

private:
  // Here we have some state which depends on the host configuration (number of channels, etc).
  std::vector<float> previous_values{};
};
}
