#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Simple gain CHOP for TouchDesigner.

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <cmath>

namespace examples
{

class GainCHOP
{
public:
  halp_meta(name, "Gain CHOP")
  halp_meta(c_name, "avnd_gain_chop")
  halp_meta(category, "Audio")
  halp_meta(author, "Avendish")
  halp_meta(description, "Simple gain/volume control for TouchDesigner")
  halp_meta(uuid, "8f3e4d2a-1b5c-4a3f-9e7d-2c1b8a4f6e3d")

  using setup = halp::setup;
  using tick = halp::tick;

  struct
  {
    halp::dynamic_audio_bus<"Input", float> audio;

    // Gain in dB.
    halp::hslider_f32<"Gain", halp::range{-60.f, 12.f, 0.f}> gain;
  } inputs;

  struct
  {
    halp::dynamic_audio_bus<"Output", float> audio;
  } outputs;

  void operator()(halp::tick t)
  {
    const float gain_db = inputs.gain.value;
    const float gain_linear = std::pow(10.f, gain_db / 20.f);

    for (int ch = 0; ch < inputs.audio.channels; ++ch)
    {
      const float* input = inputs.audio[ch];
      float* output = outputs.audio[ch];

      for (int i = 0; i < t.frames; ++i)
      {
        output[i] = input[i] * gain_linear;
      }
    }
  }
};

} // namespace examples
