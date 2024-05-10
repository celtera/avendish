#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

namespace mo
{
struct MidiGlide
{
  halp_meta(name, "Midi Glide")
  halp_meta(c_name, "midi_glide")
  halp_meta(category, "Midi");
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Midi Glide")
  halp_meta(uuid, "74d26417-1ce5-4765-874e-055921f0a0bb")

  struct
  {
    halp::midi_bus<"in", libremidi::message> in;
  } inputs;

  struct
  {
    halp::midi_bus<"out", libremidi::message> out;
  } outputs;

  void operator()()
  {
    for(const auto& note : inputs.in)
    {
      if(note.is_note_on_or_off())
      {
        auto vel = note.bytes[2];
        if(vel == 0 || note.get_message_type() == libremidi::message_type::NOTE_OFF)
        {
          if(note.bytes[1] == current_note)
          {
            outputs.out.push_back(libremidi::channel_events::note_off(note.get_channel(), current_note, 0));
            outputs.out.back().timestamp = note.timestamp;
            current_note = 255;
          }
        }
        else
        {
          // Send a note off for the current note
          if(current_note != 255)
          {
            outputs.out.push_back(libremidi::channel_events::note_off(note.get_channel(), current_note, 0));
            outputs.out.back().timestamp = note.timestamp;
          }
          outputs.out.push_back(note);
          current_note = note.bytes[1];
        }
      }
      else
      {
        outputs.out.push_back(note);
      }
    }
  }

  uint8_t current_note{255};
};
}
