#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestAudioFrame
{
  halp_meta(name, "Test audio (per-frame ports)")
  halp_meta(c_name, "avnd_test_audio_frame")
  halp_meta(category, "Tests/Audio")
  halp_meta(uuid, "0d312a10-4ccc-4778-8c11-22c07962f052")
  struct inputs
  {
    halp::dynamic_audio_frame<"In", double> audio;
    halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 2., .init = 1.}> gain;
  };
  struct outputs
  {
    halp::dynamic_audio_frame<"Out", double> audio;
  };
  void operator()(const inputs& ins, outputs& outs)
  {
    for(int i = 0; i < ins.audio.channels && i < outs.audio.channels; i++)
      outs.audio[i] = ins.gain * ins.audio[i];
  }
};
}
