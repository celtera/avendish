#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/soundfile_port.hpp>

namespace ao
{
class AudioSum
{
public:
  halp_meta(name, "Audio Sum")
  halp_meta(c_name, "audio_sum")
  halp_meta(category, "Audio/Utilities")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Sum all the input channels")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/audio-utilities.html#sum")
  halp_meta(uuid, "8938ca07-ce26-4d0a-b7f9-bff465a28a97")

  struct
  {
    halp::dynamic_audio_bus<"Input", double> audio;
  } inputs;

  struct
  {
    halp::fixed_audio_bus<"Output", double, 1> audio;
  } outputs;

  void operator()(int frames)
  {
    auto out = outputs.audio.channel(0, frames);
    for(int j = 0; j < frames; j++)
      out[j] = 0.;

    for(int i = 0; i < inputs.audio.channels; i++)
    {
      const auto in = inputs.audio.channel(i, frames);
      for(int j = 0; j < frames; j++)
        out[j] += in[j];
    }
  }
};
}
