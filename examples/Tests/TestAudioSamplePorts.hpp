#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestAudioSamplePorts
{
  halp_meta(name, "Test audio (per-sample ports)")
  halp_meta(c_name, "avnd_test_audio_sample_ports")
  halp_meta(category, "Tests/Audio")
  halp_meta(uuid, "90b6f42d-cd55-4a6c-9000-f6892ce37665")
  struct inputs
  {
    halp::audio_sample<"In", double> audio;
    halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 2., .init = 1.}> gain;
  };
  struct outputs
  {
    halp::audio_sample<"Out", double> audio;
  };
  void operator()(const inputs& ins, outputs& outs) { outs.audio = ins.gain * ins.audio; }
};
}
