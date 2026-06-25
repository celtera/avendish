#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <algorithm>

namespace examples::tests
{
struct TestAudioVariableChannels
{
  halp_meta(name, "Test audio (variable channels)")
  halp_meta(c_name, "avnd_test_audio_variable")
  halp_meta(category, "Tests/Audio")
  halp_meta(uuid, "da6ccada-d748-42ff-8435-5e58aee5cf57")
  using setup = halp::setup;
  using tick = halp::tick;
  struct
  {
    halp::fixed_audio_bus<"In", double, 2> audio;
    halp::hslider_i32<"Channels", halp::irange{.min = 1, .max = 16, .init = 2}> channels;
  } inputs;
  struct
  {
    halp::variable_audio_bus<"Out", double> audio;
  } outputs;
  void prepare(halp::setup) { outputs.audio.request_channels(inputs.channels.value); }
  void operator()(halp::tick t)
  {
    if(outputs.audio.channels != inputs.channels.value)
    {
      outputs.audio.request_channels(inputs.channels.value);
      return;
    }
    for(int i = 0; i < outputs.audio.channels; i++)
      std::copy_n(inputs.audio.samples[0], t.frames, outputs.audio.samples[i]);
  }
};
}
