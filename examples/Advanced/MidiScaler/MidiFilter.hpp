#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/midifile_port.hpp>
#include <libremidi/message.hpp>

namespace mtk
{
/**
 * This example extracts various kind of MIDI events
 */
struct MidiFilter
{
  halp_meta(name, "Midi filter")
  halp_meta(c_name, "avnd_helpers_midifilter")
  halp_meta(uuid, "f66d6cf7-693a-47d4-a249-5978cc946e43")
  halp_meta(category, "Midi")

  struct
  {
    halp::midi_bus<"MIDI messages"> midi;
    struct
    {
      halp__enum("Filter type", CC, CC, PitchBend, AfterTouch, PolyPressure);
    } filter;
    struct : halp::spinbox_i32<"Channel", halp::range{0, 16, 0}>
    {
      halp_meta(description, "Channel (0 = ALL)")
    } channel;
    struct : halp::spinbox_i32<"Index", halp::range{0, 128, 1}>
    {
      halp_meta(description, "Filter (0 = ALL)")
    } index;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI messages"> midi;
    halp::val_port<"Raw Output", int> raw;
    halp::val_port<"Normalized", float> normalized;
  } outputs;

  using tick = halp::tick_musical;
  void operator()()
  {
    for(auto& msg : inputs.midi)
    {
      libremidi::message m({msg.bytes.begin(), msg.bytes.end()}, msg.timestamp);
      if(inputs.channel != 0 && m.get_channel() == inputs.channel)
        continue;

      using filter = decltype(inputs.filter)::enum_type;
      switch(inputs.filter)
      {
        case filter::AfterTouch:
          if(m.get_message_type() == libremidi::message_type::AFTERTOUCH)
          {
            outputs.midi.push_back(msg);
            outputs.raw = m.bytes[1];
            outputs.normalized = m.bytes[1] / 127.;
          }
          break;
        case filter::CC:
          if(m.get_message_type() == libremidi::message_type::CONTROL_CHANGE)
          {
            if(inputs.index == 0 || inputs.index == m.bytes[1])
            {
              outputs.midi.push_back(msg);
              outputs.raw = m.bytes[2];
              outputs.normalized = m.bytes[2] / 127.;
            }
          }
          break;
        case filter::PolyPressure:
          if(m.get_message_type() == libremidi::message_type::POLY_PRESSURE)
          {
            if(inputs.index == 0 || inputs.index == m.bytes[1])
            {
              outputs.midi.push_back(msg);
              outputs.raw = m.bytes[2];
              outputs.normalized = m.bytes[2] / 127.;
            }
          }
          break;
        case filter::PitchBend:
          if(m.get_message_type() == libremidi::message_type::PITCH_BEND)
          {
            outputs.midi.push_back(msg);
            outputs.raw = (m.bytes[2] * 128 + m.bytes[1]);
            outputs.normalized = outputs.raw / (128. * 128.) - 0.5;
          }
          break;
      }
    }
  }
};
}
