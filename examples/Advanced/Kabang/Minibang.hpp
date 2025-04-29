#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "Kabang.hpp"
#include "Sampler.hpp"

#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

#include <vector>

namespace kbng
{
class Minibang
{
public:
  halp_meta(name, "Minibang")
  halp_meta(category, "Audio/Synth")
  halp_meta(c_name, "minibang")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Basic MIDI drum sampler")
  halp_meta(uuid, "686cb911-42ad-4bc6-a058-1962e746367f")

  struct ins
  {
    halp::midi_bus<"Input"> midi;
    halp::knob_f32<"Volume"> volume;

    DrumChannel s1;

  } inputs;

  struct
  {
    halp::fixed_audio_bus<"Output", double, 2> audio;
  } outputs;

  using type = const DrumChannel&;
  void for_each_channel(auto&& f) { f(inputs.s1); }

  using tick = halp::tick;
  void operator()(halp::tick t);

  struct ui
  {
    halp_meta(name, "Main")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::mid)

    struct : halp::recursive_group_item<&ins::s1, DrumChannel::ui>
    {
      halp_meta(name, "Drum 1")
      halp_meta(background, halp::colors::darker)

    } s1;

    struct
    {
      halp_meta(name, "Global")
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::darker)

      halp::item<&ins::volume> globalvol;
    } global;
  };
};
}
