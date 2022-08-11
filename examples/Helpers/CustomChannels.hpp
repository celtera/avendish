#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <functional>

namespace examples::helpers
{

/**
 * Example of how one may control the channels.
 */
class CustomChannels
{
public:
  halp_meta(name, "Multichannel (helpers)")
  halp_meta(c_name, "avnd_helpers_multichannel")
  halp_meta(uuid, "814d70ef-c675-41de-b4db-997323feb7cf")

  using setup = halp::setup;
  using tick = halp::tick;

  struct
  {
    halp::fixed_audio_bus<"Input", double, 2> audio;
    halp::hslider_i32<"Channels", halp::range{.min = 0., .max = 100., .init = 1.}>
        channels;
  } inputs;

  struct
  {
    halp::variable_audio_bus<"Output", double> audio;
  } outputs;

  void prepare(halp::setup s) { outputs.audio.request_channels(inputs.channels.value); }

  void operator()(halp::tick t)
  {
    if(outputs.audio.channels != inputs.channels.value)
    {
      outputs.audio.request_channels(inputs.channels.value);
      return;
    }

    for(int i = 0; i < outputs.audio.channels; i++)
    {
      std::copy_n(inputs.audio.samples[0], t.frames, outputs.audio.samples[i]);
    }
  }
};
}
