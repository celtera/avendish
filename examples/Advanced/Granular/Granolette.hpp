#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <vector>

namespace oscr
{
class Granolette
{
public:
  halp_meta(name, "Granolette")
  halp_meta(c_name, "granolette")
  halp_meta(uuid, "a8ffe1d1-152d-4bfc-9209-93cf8c0453ca")

  using setup = halp::setup;
  using tick = halp::tick;

  struct
  {
    halp::soundfile_port<"Sound"> sound;
    halp::hslider_f32<"Position", halp::range{0., 1., 0.}> pos;
  } inputs;

  struct
  {
    halp::variable_audio_bus<"Output", double> audio;
  } outputs;

  void prepare(halp::setup s) {
    if(inputs.sound)
      outputs.audio.request_channels(inputs.sound.channels());
  }

  void operator()(halp::tick t)
  {
    if(!inputs.sound)
      return;

    if(outputs.audio.channels != inputs.sound.channels())
      outputs.audio.request_channels(inputs.sound.channels());

    // Just take the first channel of the soundfile.
    // in is a std::span
    const auto in = inputs.sound.channel(0);

    // We'll read at this position
    const int64_t start = std::floor(inputs.pos * inputs.sound.frames());

    // Copy it at the given position for each output
    for(int i = 0; i < outputs.audio.channels; i++)
    {
      // Output buffer for channel i, also a std::span.
      auto out = outputs.audio.channel(i, t.frames);

      for(int j = 0; j < t.frames; j++)
      {
        // If we're before the end of the file copy the sample
        if(start + j < inputs.sound.frames())
          out[j] = in[start + j];
        else
          out[j] = 0;
      }
    }
  }
};
}
