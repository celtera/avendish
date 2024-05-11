#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/soundfile_port.hpp>

namespace ao
{
class Silence
{
public:
  halp_meta(name, "Silence")
  halp_meta(c_name, "silence")
  halp_meta(category, "Audio")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Enjoy the silence")
  halp_meta(uuid, "3bf48e32-06c0-4693-8a80-062ae9b5eac8")

  struct
  {
    halp::spinbox_i32<"Channels"> channels;
  } inputs;

  struct
  {
    halp::variable_audio_bus<"Output", double> audio;
  } outputs;

  using setup = halp::setup;
  void prepare(halp::setup s) { outputs.audio.request_channels(inputs.channels); }

  void operator()()
  {
    if(outputs.audio.channels != inputs.channels)
    {
      outputs.audio.request_channels(inputs.channels);
      return;
    }
  }
};
}
