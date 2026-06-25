#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestAudioBusFixed
{
  halp_meta(name, "Test audio (fixed stereo bus)")
  halp_meta(c_name, "avnd_test_audio_bus_fixed")
  halp_meta(category, "Tests/Audio")
  halp_meta(uuid, "0a29b9bc-8cb7-46c7-ac42-ebf7c055207c")
  struct
  {
    halp::fixed_audio_bus<"In", double, 2> audio;
    halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 2., .init = 1.}> gain;
  } inputs;
  struct
  {
    halp::fixed_audio_bus<"Out", double, 2> audio;
  } outputs;
  void operator()(int frames)
  {
    for(int c = 0; c < inputs.audio.channels(); ++c)
      for(int k = 0; k < frames; k++)
        outputs.audio[c][k] = inputs.gain * inputs.audio[c][k];
  }
};
}
