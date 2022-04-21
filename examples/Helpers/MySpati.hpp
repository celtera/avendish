#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <vector>

namespace Spat
{

/**
 * Same as the "simple" example, but with the helpers library
 */
struct Spati
{
public:
  halp_meta(name, "Spati example")
  halp_meta(c_name, "avnd_example")
  halp_meta(uuid, "82bdb9b5-9cf8-440e-8675-c0caf4fc59b9")

  using setup = halp::setup;
  using tick = halp::tick;

  struct
  {
    halp::dynamic_audio_bus<"Input", double> audio;
    halp::hslider_f32<"Weight", halp::range{.min = 0., .max = 1., .init = 0.5}> weight;
  } inputs;

  struct
  {
    halp::dynamic_audio_bus<"Output", double> audio;
  } outputs;

  void prepare(halp::setup info) { previous_values.resize(info.input_channels); }

  // Do our processing for N samples
  void operator()(halp::tick t)
  {
    // Process the input buffer
    for (int i = 0; i < inputs.audio.channels; i++)
    {
      auto* in = inputs.audio[i];
      auto* out = outputs.audio[i];

      float& prev = this->previous_values[i];

      for (int j = 0; j < t.frames; j++)
      {
        out[j] = inputs.weight * in[j];// + (1.0 - inputs.weight) * prev;
        prev = out[j];
      }
    }
  }

private:
  // Here we have some state which depends on the host configuration (number of channels, etc).
  std::vector<float> previous_values{};
};
}
