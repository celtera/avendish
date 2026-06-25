#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestAudioGainMono
{
  halp_meta(name, "Test audio gain (mono)")
  halp_meta(c_name, "avnd_test_audio_mono")
  halp_meta(category, "Tests/Audio")
  halp_meta(uuid, "b6412f44-fd88-48a4-89d1-2edb0e02c90a")
  struct { halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 2., .init = 1.}> gain; } inputs;
  float operator()(float in) { return in * inputs.gain; }
};
}
