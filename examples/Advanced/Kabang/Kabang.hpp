#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "Sampler.hpp"

#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

#include <vector>

namespace kbng
{
struct DrumChannel
{
  halp_flag(recursive_group);

  sample_port<"Drum"> sample;
  halp::knob_f32<"Volume", halp::range{0.0, 2., 1.}> vol;
  halp::knob_f32<"Pitch", halp::range{0.01, 10, 1.}> pitch;
  halp::spinbox_i32<"Input", halp::range{0, 127, 38}> midi_key;

  void run(int frames, int channels, double** out, double volume)
  {
    sample.sample(frames, channels, out, this->pitch, this->vol * volume);
  }

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)

    halp::item<&DrumChannel::sample> sample;

    struct
    {
      halp_meta(layout, halp::layouts::grid)
      halp_meta(columns, 2)
      halp::item<&DrumChannel::vol> vol;
      halp::item<&DrumChannel::pitch> pitch;
      halp::item<&DrumChannel::midi_key> midi_key;
    } controls;
  };
};

class Kabang
{
public:
  halp_meta(name, "Kabang")
  halp_meta(c_name, "kabang")
  halp_meta(uuid, "d6cd303e-c851-4655-806e-6c344cade2ae")

  struct ins
  {
    halp::midi_bus<"Input"> midi;

    DrumChannel s1;
    DrumChannel s2;
    DrumChannel s3;
    DrumChannel s4;
    DrumChannel s5;
    DrumChannel s6;
    DrumChannel s7;
    DrumChannel s8;

    halp::knob_f32<"Volume"> volume;
  } inputs;

  struct
  {
    halp::fixed_audio_bus<"Output", double, 2> audio;
  } outputs;

  using type = const DrumChannel&;
  void for_each_channel(auto&& f)
  {
    f(inputs.s1);
    f(inputs.s2);
    f(inputs.s3);
    f(inputs.s4);
    f(inputs.s5);
    f(inputs.s6);
    f(inputs.s7);
    f(inputs.s8);
  }

  using tick = halp::tick;
  void operator()(halp::tick t)
  {
    for(auto& msg : inputs.midi)
    {
      auto m = libremidi::message{{msg.bytes[0], msg.bytes[1], msg.bytes[2]}};
      if(m.get_message_type() == libremidi::message_type::NOTE_ON)
      {
        for_each_channel([&msg](DrumChannel& channel) {
          if(msg.bytes[1] == channel.midi_key)
            channel.sample.trigger(msg.timestamp, msg.bytes[2]);
        });
      }
    }
    for_each_channel([&](DrumChannel& channel) {
      channel.run(t.frames, 2, outputs.audio.samples, inputs.volume);
    });
  }

  struct ui
  {
    halp_meta(name, "Main")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::mid)

    struct
    {
      halp_meta(name, "Tabs")
      halp_meta(layout, halp::layouts::tabs)
      halp_meta(background, halp::colors::darker)

      struct : halp::recursive_group_item<&ins::s1, DrumChannel::ui>
      {
        halp_meta(name, "Drum 1")
      } s1;
      struct : halp::recursive_group_item<&ins::s2, DrumChannel::ui>
      {
        halp_meta(name, "Drum 2")
      } s2;
      struct : halp::recursive_group_item<&ins::s3, DrumChannel::ui>
      {
        halp_meta(name, "Drum 3")
      } s3;
      struct : halp::recursive_group_item<&ins::s4, DrumChannel::ui>
      {
        halp_meta(name, "Drum 4")
      } s4;
      struct : halp::recursive_group_item<&ins::s1, DrumChannel::ui>
      {
        halp_meta(name, "Drum 5")
      } s5;
      struct : halp::recursive_group_item<&ins::s2, DrumChannel::ui>
      {
        halp_meta(name, "Drum 6")
      } s6;
      struct : halp::recursive_group_item<&ins::s3, DrumChannel::ui>
      {
        halp_meta(name, "Drum 7")
      } s7;
      struct : halp::recursive_group_item<&ins::s4, DrumChannel::ui>
      {
        halp_meta(name, "Drum 8")
      } s8;
    } drum_tabs;

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
