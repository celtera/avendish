#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

#include <cstdint>

namespace mo
{
/**
 * Monophonic legato: one note sounds at a time. A new note-on ends the note
 * that sounds, a note-off ends it only if it is its own key's.
 *
 * A note is identified by its channel and its pitch: its note-off goes on the
 * channel its note-on went on, and a note-off of the same pitch on another
 * channel leaves it sounding.
 */
struct MidiGlide
{
  halp_meta(name, "Midi Glide")
  halp_meta(c_name, "midi_glide")
  halp_meta(category, "Midi/Pitch");
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Midi Glide")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/midi-utilities.html#midi-glide")
  halp_meta(uuid, "74d26417-1ce5-4765-874e-055921f0a0bb")
  halp_flag(process_exec);

  struct
  {
    halp::midi_bus<"in", libremidi::message> in;
  } inputs;

  struct
  {
    halp::midi_bus<"out", libremidi::message> out;
  } outputs;

  //! The sounding note ends at the start of the next tick.
  void stop() noexcept { m_stop_requested = true; }

  void operator()() noexcept
  {
    if(m_stop_requested)
    {
      m_stop_requested = false;
      end_current(0, 0);
    }

    for(const auto& m : inputs.in)
    {
      if(m.is_note_on_or_off())
      {
        const int channel = m.get_channel();
        const uint8_t pitch = m.bytes[1];
        const uint8_t vel = m.bytes[2];
        const bool release
            = vel == 0 || m.get_message_type() == libremidi::message_type::NOTE_OFF;
        if(release)
        {
          if(sounding() && pitch == m_note && channel == m_channel)
          {
            const bool is_off = m.get_message_type() == libremidi::message_type::NOTE_OFF;
            end_current(is_off ? vel : 0, m.timestamp);
          }
        }
        else
        {
          end_current(0, m.timestamp);
          outputs.out.push_back(m);
          m_note = pitch;
          m_channel = channel;
        }
      }
      else
      {
        // All sound off and all notes off end the note at the receiver.
        if(m.get_message_type() == libremidi::message_type::CONTROL_CHANGE
           && (m.bytes[1] == 120 || m.bytes[1] == 123) && m.get_channel() == m_channel)
          m_note = no_note;
        outputs.out.push_back(m);
      }
    }
  }

  //! Whether a note sounds at the output.
  bool sounding() const noexcept { return m_note != no_note; }

private:
  static constexpr uint8_t no_note = 255;

  void end_current(uint8_t velocity, int64_t ts) noexcept
  {
    if(!sounding())
      return;
    outputs.out.push_back(libremidi::channel_events::note_off(m_channel, m_note, velocity));
    outputs.out.back().timestamp = ts;
    m_note = no_note;
  }

  uint8_t m_note{no_note};
  int m_channel{1};
  bool m_stop_requested{};
};
}
