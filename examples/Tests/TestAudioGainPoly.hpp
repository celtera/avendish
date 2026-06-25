#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <cstdint>

namespace examples::tests
{
struct TestAudioGainPoly
{
  halp_meta(name, "Test audio gain (poly)")
  halp_meta(c_name, "avnd_test_audio_poly")
  halp_meta(category, "Tests/Audio")
  halp_meta(uuid, "71385b49-a2a6-4ada-a295-034ebe9c1fb1")
  struct
  {
    halp::audio_input_bus<"In"> audio;
    halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 2., .init = 1.}> gain;
  } inputs;
  struct { halp::audio_output_bus<"Out"> audio; } outputs;

  void operator()(int64_t N)
  {
    const float g = inputs.gain;
    const auto chans = inputs.audio.channels;
    for(int c = 0; c < chans; c++)
    {
      auto in = inputs.audio.channel(c, N);
      auto out = outputs.audio.channel(c, N);
      for(int64_t i = 0; i < N; i++)
        out[i] = in[i] * g;
    }
  }
};
}
