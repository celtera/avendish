#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace mtk
{
/**
 * @brief A sustain pedal applied to the MIDI stream: note-offs are held back
 * while the pedal is down, and sent when it goes up.
 *
 * The pedal is CC 64 of each channel (64 and above is down), or the "Pedal"
 * control, which holds every channel. Modes:
 * - All: every note released while the pedal is down rings until it goes up.
 * - Chord: as All, but starting a new chord (a note-on while no key is held
 *   on the channel) ends the notes the pedal holds, so only the last chord
 *   rings.
 * - Sostenuto: only the notes whose key is down when the pedal goes down ring.
 * - Semi-sostenuto: the notes whose key is down when the pedal goes down ring
 *   until it goes up, and the notes played afterwards ring as in Chord.
 *
 * A note the pedal holds that is struck again is ended before its new note-on,
 * so that every note-on sent gets one note-off.
 */
struct MidiSustain
{
  halp_meta(name, "Midi sustain")
  halp_meta(c_name, "midi_sustain")
  halp_meta(category, "Midi/Timing")
  halp_meta(author, "ossia score")
  halp_meta(description, "Hold notes after their key is released, like a sustain pedal")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-sustain.html")
  halp_meta(uuid, "943f891c-e56f-4e60-a270-3420657fe368")

  enum Mode : uint8_t
  {
    All,
    Chord,
    SemiSostenuto,
    Sostenuto
  };

  struct ins
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;

    struct : halp::enum_t<Mode, "Mode">
    {
      halp_meta(
          description,
          "All holds every note; Chord only the last chord; Sostenuto only the "
          "notes held when the pedal goes down; Semi-sostenuto those, and the "
          "last chord played after")
      struct range
      {
        std::string_view values[4] = {"All", "Chord", "Semi-sostenuto", "Sostenuto"};
        Mode init = All;
      };
    } mode;

    struct : halp::toggle<"Pedal">
    {
      halp_meta(description, "Holds the pedal down on every channel, as CC 64 does on one")
    } pedal;

    struct : halp::toggle<"Pass CC 64", halp::toggle_setup{.init = true}>
    {
      halp_meta(description, "Send the incoming CC 64 on, or keep it")
    } pass_pedal;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;
  } outputs;

  void stop() noexcept { m_stop_requested = true; }

  using tick = halp::tick;
  void operator()(halp::tick t) noexcept
  {
    const int frames = t.frames > 0 ? t.frames : 0;
    if(m_stop_requested)
    {
      m_stop_requested = false;
      release_all(0);
      m_cc_down = {};
      m_pedal_down = {};
    }

    for(int c = 0; c < 16; c++)
      update_pedal(c, 0);

    for(const auto& m : inputs.midi)
    {
      const int64_t ts = frames > 0 ? std::clamp<int64_t>(m.timestamp, 0, frames - 1) : 0;
      handle(m, ts);
    }
  }

  //! Whether a note sounds at the output, held by its key or by the pedal.
  bool sounding(int channel, int pitch) const noexcept
  {
    const auto& n = m_notes[(channel - 1) & 15][pitch & 127];
    return n.key || n.sustained;
  }

private:
  struct note_state
  {
    bool key{};       //!< its key is down
    bool sustained{}; //!< its key is up, the pedal holds it
    bool latched{};   //!< its key was down when the pedal went down
    uint8_t release_velocity{};
  };

  void send(libremidi::message m, int64_t ts) noexcept
  {
    m.timestamp = ts;
    outputs.midi.push_back(std::move(m));
  }

  void end_note(int c, int p, int64_t ts) noexcept
  {
    auto& n = m_notes[c][p];
    if(n.key)
      m_keys_down[c]--;
    send(libremidi::channel_events::note_off(c + 1, p, n.release_velocity), ts);
    n = {};
  }

  void handle(const libremidi::message& m, int64_t ts) noexcept
  {
    const int c = channel_of(m) - 1;
    switch(classify(m))
    {
      case midi_kind::note_on:
        note_on(m, c, m.bytes[1] & 127, ts);
        return;
      case midi_kind::note_off:
        if(m.size() >= 3)
        {
          const int vel = (m.bytes[0] & 0xF0) == 0x80 ? (m.bytes[2] & 127) : 0;
          note_off(c, m.bytes[1] & 127, vel, ts);
          return;
        }
        break;
      case midi_kind::control_change:
        if(m.size() < 3)
          break;
        if(m.bytes[1] == 64)
        {
          m_cc_down[c] = m.bytes[2] >= 64;
          if(inputs.pass_pedal.value)
            send(m, ts);
          update_pedal(c, ts);
          return;
        }
        if(m.bytes[1] == 121)
        {
          // Reset all controllers lifts the pedal.
          m_cc_down[c] = false;
          send(m, ts);
          update_pedal(c, ts);
          return;
        }
        if(is_all_notes_off(m))
        {
          for(int p = 0; p < 128; p++)
            if(m_notes[c][p].key || m_notes[c][p].sustained)
              end_note(c, p, ts);
        }
        break;
      default:
        break;
    }
    send(m, ts);
  }

  bool held_by_pedal(const note_state& n, int mode) const noexcept
  {
    switch(mode)
    {
      case Sostenuto:
        return n.latched;
      case All:
      case Chord:
      case SemiSostenuto:
      default:
        return true;
    }
  }

  void note_on(const libremidi::message& m, int c, int p, int64_t ts) noexcept
  {
    auto& n = m_notes[c][p];
    if(n.key || n.sustained)
      end_note(c, p, ts);

    const int mode = inputs.mode.value;
    if(m_pedal_down[c] && m_keys_down[c] == 0 && (mode == Chord || mode == SemiSostenuto))
    {
      // A new chord: the previous one stops ringing.
      for(int k = 0; k < 128; k++)
        if(m_notes[c][k].sustained && !m_notes[c][k].latched)
          end_note(c, k, ts);
    }

    n.key = true;
    m_keys_down[c]++;
    send(m, ts);
  }

  void note_off(int c, int p, int vel, int64_t ts) noexcept
  {
    auto& n = m_notes[c][p];
    if(!n.key)
      return;
    n.release_velocity = uint8_t(vel);
    if(m_pedal_down[c] && held_by_pedal(n, inputs.mode.value))
    {
      n.key = false;
      n.sustained = true;
      m_keys_down[c]--;
      return;
    }
    end_note(c, p, ts);
  }

  //! Applies a change of the pedal of channel c (CC or control).
  void update_pedal(int c, int64_t ts) noexcept
  {
    const bool down = m_cc_down[c] || inputs.pedal.value;
    if(down == m_pedal_down[c])
      return;
    m_pedal_down[c] = down;
    if(down)
    {
      for(auto& n : m_notes[c])
        n.latched = n.key;
    }
    else
    {
      for(int p = 0; p < 128; p++)
      {
        auto& n = m_notes[c][p];
        n.latched = false;
        if(n.sustained)
          end_note(c, p, ts);
      }
    }
  }

  void release_all(int64_t ts) noexcept
  {
    for(int c = 0; c < 16; c++)
      for(int p = 0; p < 128; p++)
        if(m_notes[c][p].key || m_notes[c][p].sustained)
          end_note(c, p, ts);
    m_keys_down = {};
  }

  std::array<std::array<note_state, 128>, 16> m_notes{};
  std::array<int, 16> m_keys_down{};
  std::array<bool, 16> m_cc_down{};
  std::array<bool, 16> m_pedal_down{};
  bool m_stop_requested{};
};
}
