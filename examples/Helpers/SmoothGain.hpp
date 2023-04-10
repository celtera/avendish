#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/smoothers.hpp>

#include <vector>

namespace examples::helpers
{
/**
 * Smooth gain
 */
class SmoothGainPoly
{
public:
  halp_meta(name, "Smooth Gain")
  halp_meta(c_name, "avnd_helpers_smooth_gain")
  halp_meta(uuid, "032e1734-f84a-4eb2-9d14-01fc3dea4c14")

  using setup = halp::setup;
  using tick = halp::tick;

  struct
  {
    halp::dynamic_audio_bus<"Input", double> audio;
    struct : halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 1., .init = 0.5}>
    {
      struct smoother
      {
        float milliseconds = 20.;
      };
    } gain;
  } inputs;

  struct
  {
    halp::dynamic_audio_bus<"Output", double> audio;
  } outputs;

  void prepare(halp::setup info) { }

  // Do our processing for N samples
  void operator()(halp::tick t)
  {
    // Process the input buffer
    for(int i = 0; i < inputs.audio.channels; i++)
    {
      auto* in = inputs.audio[i];
      auto* out = outputs.audio[i];

      for(int j = 0; j < t.frames; j++)
      {
        out[j] = inputs.gain * in[j];
      }
    }
  }
};
class SmoothGain
{
public:
  halp_meta(name, "Smooth Gain")
  halp_meta(c_name, "avnd_helpers_smooth_gain")
  halp_meta(uuid, "032e1734-f84a-4eb2-9d14-01fc3dea4c14")

  struct inputs
  {
    halp::audio_sample<"Input", double> audio;
    struct : halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 1., .init = 0.5}>
    {
      using smoother = halp::milliseconds_smooth<20>;
    } gain;
  };

  struct outputs
  {
    halp::audio_sample<"Output", double> audio;
  };

  // Do our processing for N samples
  void operator()(const inputs& inputs, outputs& outputs)
  {
    outputs.audio.sample = inputs.audio.sample * inputs.gain;
  }
};
}
