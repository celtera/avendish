#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/soundfile_port.hpp>

#include <vector>

namespace ao
{
class Soundfile
{
public:
  halp_meta(name, "Soundfile")
  halp_meta(c_name, "soundfile")
  halp_meta(category, "Audio")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Basic soundfile player")
  halp_meta(uuid, "5c12a267-63a3-4dde-99e5-264b4ca4243b")

  struct
  {
    halp::soundfile_port<"Sound"> sound;
  } inputs;

  struct
  {
    halp::variable_audio_bus<"Output", double> audio;
  } outputs;

  using setup = halp::setup;
  void prepare(halp::setup s)
  {
    if(inputs.sound)
      outputs.audio.request_channels(inputs.sound.channels());
  }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical t)
  {
    if(!inputs.sound)
    {
      qDebug() << "outputs.audio.channels << inputs.sound.channels()";
      return;
    }

    if(outputs.audio.channels != inputs.sound.channels())
    {
      outputs.audio.request_channels(inputs.sound.channels());
      return;
    }

    // Just take the first channel of the soundfile.
    // in is a std::span

    // We'll read at this position
    const int64_t start = t.position_in_frames;

    // Copy it at the given position for each output
    for(int i = 0; i < outputs.audio.channels; i++)
    {
      const auto in = inputs.sound.channel(i);
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
