#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

namespace mo
{
struct MidiPitch
{
  halp_meta(name, "Midi Pitch")
  halp_meta(c_name, "PitchToValue")
  halp_meta(category, "Midi");
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Extract a MIDI pitch")
  halp_meta(uuid, "29ce484f-cb56-4501-af79-88768fa261c3")

  struct
  {
    halp::midi_bus<"in", libremidi::message> in;
  } inputs;

  struct
  {
    struct : halp::timed_callback<"out", int>
    {
      halp_meta(unit, "midipitch")
    } out;
  } outputs;

  void operator()()
  {
    for(const auto& note : inputs.in)
    {
      if(note.get_message_type() == libremidi::message_type::NOTE_ON)
        outputs.out(note.timestamp, note.bytes[1]);
    }
  }
};
}
