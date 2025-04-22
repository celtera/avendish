#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

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
template <halp::static_string lit, auto setup>
struct log_pot : halp::knob_f32<lit, setup>
{
  using mapper = halp::log_mapper<std::ratio<85, 100>>;
};

struct DrumChannel
{
  halp_flag(recursive_group);

  sample_port<"Drum"> sample;

  halp::knob_f32<"Volume", halp::range{0.0, 2., 1.}> vol;
  halp::knob_f32<"Pitch", halp::range{0.25, 5, 1.}> pitch;
  halp::spinbox_i32<"Input", halp::range{0, 127, 38}> midi_key;

  log_pot<"HP Cutoff", halp::range{20., 20000., 30.}> hp_cutoff;
  halp::knob_f32<"HP Reso", halp::range{0.001, 1., 0.5}> hp_res;

  log_pot<"LP Cutoff", halp::range{20., 20000., 20000.}> lp_cutoff;
  halp::knob_f32<"LP Reso", halp::range{0.001, 1., 0.5}> lp_res;

  halp::toggle<"P. Env"> pitch_env_enable;
  log_pot<"P. Attack", halp::range{0.0001, 1., 0.01}> pitch_attack;
  log_pot<"P. Decay", halp::range{0.0001, 1., 0.05}> pitch_decay;
  halp::knob_f32<"P. Sustain", halp::range{0., 1., 1.}> pitch_sustain;
  log_pot<"P. Release", halp::range{0.0001, 10., 0.25}> pitch_release;
  halp::knob_f32<"Vel->Pitch", halp::range{-1., 1., 0.}> pitch_envelop;

  halp::toggle<"F. Env"> filt_env_enable;
  log_pot<"F. Attack", halp::range{0.0001, 1., 0.01}> filt_attack;
  log_pot<"F. Decay", halp::range{0.0001, 1., 0.05}> filt_decay;
  halp::knob_f32<"F. Sustain", halp::range{0., 1., 1.}> filt_sustain;
  log_pot<"F. Release", halp::range{0.0001, 10., 0.25}> filt_release;
  halp::knob_f32<"Vel->Filt", halp::range{-1., 1., 0.}> filt_envelop;

  halp::toggle<"Amp. Env"> amp_env_enable;
  log_pot<"Attack", halp::range{0.0001, 1., 0.01}> amp_attack;
  log_pot<"Decay", halp::range{0.0001, 1., 0.05}> amp_decay;
  halp::knob_f32<"Sustain", halp::range{0., 1., 1.}> amp_sustain;
  log_pot<"Release", halp::range{0.0001, 10., 0.25}> amp_release;

  halp::knob_f32<"Vel->Amp", halp::range{-1., 1., 0.}> amp_envelop;

  void trigger(int ts, int vel_byte);

  void run(int frames, int channels, double** out, double volume);

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)

    struct
    {
      halp_meta(layout, halp::layouts::hbox)
      halp::item<&DrumChannel::vol> vol;
      halp::item<&DrumChannel::midi_key> midi_key;
      halp::item<&DrumChannel::sample> sample;
    } sample;

    struct
    {
      halp_meta(layout, halp::layouts::grid)
      halp_meta(columns, 6)

      halp::item<&DrumChannel::pitch> pitch;
      halp::label label;
      halp::item<&DrumChannel::hp_cutoff> hp_cutoff;
      halp::item<&DrumChannel::hp_res> hp_res;
      halp::item<&DrumChannel::lp_cutoff> lp_cutoff;
      halp::item<&DrumChannel::lp_res> lp_res;

      halp::item<&DrumChannel::pitch_env_enable> pitch_e;
      halp::item<&DrumChannel::pitch_attack> pitch_a;
      halp::item<&DrumChannel::pitch_decay> pitch_d;
      halp::item<&DrumChannel::pitch_sustain> pitch_s;
      halp::item<&DrumChannel::pitch_release> pitch_r;
      halp::item<&DrumChannel::pitch_envelop> pitch_env;

      halp::item<&DrumChannel::filt_env_enable> filt_e;
      halp::item<&DrumChannel::filt_attack> filt_a;
      halp::item<&DrumChannel::filt_decay> filt_d;
      halp::item<&DrumChannel::filt_sustain> filt_s;
      halp::item<&DrumChannel::filt_release> filt_r;
      halp::item<&DrumChannel::filt_envelop> filt_env;

      halp::item<&DrumChannel::amp_env_enable> amp_e;
      halp::item<&DrumChannel::amp_attack> amp_a;
      halp::item<&DrumChannel::amp_decay> amp_d;
      halp::item<&DrumChannel::amp_sustain> amp_s;
      halp::item<&DrumChannel::amp_release> amp_r;
      halp::item<&DrumChannel::amp_envelop> amp_env;

    } controls;
  };
};

class Kabang
{
public:
  halp_meta(name, "Kabang")
  halp_meta(category, "Audio/Synth")
  halp_meta(c_name, "kabang")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Basic MIDI drum sampler")
  halp_meta(uuid, "d6cd303e-c851-4655-806e-6c344cade2ae")

  struct ins
  {
    halp::midi_bus<"Input"> midi;
    halp::knob_f32<"Volume"> volume;

    DrumChannel s1;
    DrumChannel s2;
    DrumChannel s3;
    DrumChannel s4;
    DrumChannel s5;
    DrumChannel s6;
    DrumChannel s7;
    DrumChannel s8;

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
  void operator()(halp::tick t);

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

      struct : halp::recursive_group_item<&ins::s5, DrumChannel::ui>
      {
        halp_meta(name, "Drum 5")
      } s5;
      struct : halp::recursive_group_item<&ins::s6, DrumChannel::ui>
      {
        halp_meta(name, "Drum 6")
      } s6;
      struct : halp::recursive_group_item<&ins::s7, DrumChannel::ui>
      {
        halp_meta(name, "Drum 7")
      } s7;
      struct : halp::recursive_group_item<&ins::s8, DrumChannel::ui>
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
