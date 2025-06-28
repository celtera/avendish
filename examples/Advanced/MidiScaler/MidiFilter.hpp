#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cmath>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/midifile_port.hpp>
#include <libremidi/message.hpp>
#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/small_flat_map.hpp>
#include <ossia/detail/small_vector.hpp>
#include <ossia/detail/variant.hpp>

namespace mtk
{
/**
 * This example extracts various kind of MIDI events
 */
struct MidiFilter
{
  halp_meta(name, "Midi filter")
  halp_meta(c_name, "avnd_helpers_midifilter")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-filter.html")
  halp_meta(uuid, "f66d6cf7-693a-47d4-a249-5978cc946e43")
  halp_meta(category, "Midi")

  enum Mode
  {
    Index,
    Value,
    Both
  };
  enum Type
  {
    CC,
    PitchBend,
    AfterTouch,
    PolyPressure,
    NoteOn,
    NoteOff,
    NoteAny,
    NoteRunning
  };

  struct note_int
  {
    int note{};
    int velocity{};
  };
  struct note_float
  {
    int note{};
    float velocity{};
  };

  struct
  {
    halp::midi_bus<"MIDI messages", libremidi::message> midi;
    halp::enum_t<Type, "Filter type"> filter;
    struct : halp::spinbox_i32<"Channel", halp::range{0, 16, 0}>
    {
      halp_meta(description, "Channel (0 = ALL)")
    } channel;
    struct : halp::spinbox_i32<"Index", halp::range{0, 128, 0}>
    {
      halp_meta(description, "Filter (0 = ALL)")
    } index;
    halp::enum_t<Mode, "Mode"> mode;
    halp::toggle<"Note off to zero"> note_off_to_zero;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI messages", libremidi::message> midi;
    halp::timed_callback<"Raw Output", ossia::variant<int, note_int>> raw;
    halp::timed_callback<"Normalized value", ossia::variant<float, note_float>>
        normalized;

    halp::timed_callback<
        "Raw poly output",
        ossia::variant<
            ossia::small_vector<uint8_t, 128>, ossia::small_vector<note_int, 128>>>
        poly_raw;
  } outputs;

  ossia::small_flat_map<int, int, 128> running;
  ossia::small_vector<uint8_t, 128> out_no_vel;
  ossia::small_vector<note_int, 128> out_vel;

  using tick = halp::tick_musical;
  void push_poly(int ts)
  {
    out_no_vel.clear();
    out_vel.clear();
    switch(inputs.mode)
    {
      case Both: {
        for(auto [note, vel] : this->running)
          out_vel.push_back({note, vel});
        outputs.poly_raw(ts, out_vel);
        break;
      }
      case Index: {
        for(auto [note, vel] : this->running)
          out_no_vel.push_back(note);
        outputs.poly_raw(ts, out_no_vel);
        break;
      }
      case Value: {
        for(auto [note, vel] : this->running)
          out_no_vel.push_back(vel);
        outputs.poly_raw(ts, out_no_vel);
        break;
      }
    }
  }

  void push_poly_with_note_offs(int ts, int note_off)
  {
    out_no_vel.clear();
    out_vel.clear();
    switch(inputs.mode)
    {
      case Both: {
        for(auto [note, vel] : this->running)
          out_vel.push_back({note, vel});
        out_vel.push_back({note_off, 0});
        outputs.poly_raw(ts, out_vel);
        break;
      }
      case Index: {
        for(auto [note, vel] : this->running)
          out_no_vel.push_back(note);
        out_no_vel.push_back(note_off);
        outputs.poly_raw(ts, out_no_vel);
        break;
      }
      case Value: {
        for(auto [note, vel] : this->running)
          out_no_vel.push_back(note);
        out_no_vel.push_back(note_off);
        outputs.poly_raw(ts, out_no_vel);
        break;
      }
    }
  }

  void output_2_bytes(const libremidi::message& m)
  {
    const int pitch = (int)m.bytes[1];
    const int vel = (int)m.bytes[2];
    if(inputs.index == 0 || inputs.index == pitch)
    {
      outputs.midi.push_back(m);
      switch(inputs.mode)
      {
        case Both: {
          outputs.raw(m.timestamp, note_int{pitch, vel});
          outputs.normalized(m.timestamp, note_float{pitch, vel / 127.f});
          break;
        }
        case Index: {
          outputs.raw(m.timestamp, pitch);
          outputs.normalized(m.timestamp, vel / 127.);
          break;
        }
        case Value: {
          outputs.raw(m.timestamp, vel);
          outputs.normalized(m.timestamp, vel / 127.);
          break;
        }
      }
    }
  }

  void operator()()
  {
    for(auto& m : inputs.midi)
    {
      if(inputs.channel != 0 && m.get_channel() == inputs.channel)
        continue;

      const auto t = m.get_message_type();
      switch(inputs.filter)
      {
        case Type::NoteOn:
        note_on:
          if(t == libremidi::message_type::NOTE_ON)
          {
            running[m.bytes[1]] = m.bytes[2];
            output_2_bytes(m);
            push_poly(m.timestamp);
          }
          break;
        case Type::NoteOff:
        note_off:
          if(t == libremidi::message_type::NOTE_OFF)
          {
            running.erase(m.bytes[1]);
            output_2_bytes(m);
            push_poly(m.timestamp);
          }
          break;

        // In this case we transmit note ons / note offs normally
        case Type::NoteAny:
          if(t == libremidi::message_type::NOTE_ON)
            goto note_on;
          else if(t == libremidi::message_type::NOTE_OFF)
            goto note_off;

        // In this case we want the poly array to inform us about the running notes
        // In particular we take note off into account but we don't output them other than at zero if requested.
        // They will still be output from the midi out.
        case Type::NoteRunning:
          if(t == libremidi::message_type::NOTE_ON)
            goto note_on;
          else if(t == libremidi::message_type::NOTE_OFF)
          {
            const int pitch = (int)m.bytes[1];
            running.erase(pitch);

            if(inputs.index == 0 || inputs.index == m.bytes[1])
            {
              outputs.midi.push_back(m);
              if(inputs.note_off_to_zero)
              {
                switch(inputs.mode)
                {
                  case Both: {
                    outputs.raw(m.timestamp, note_int{pitch, 0});
                    outputs.normalized(m.timestamp, note_float{pitch, 0.f});
                    break;
                  }
                  case Index: {
                    outputs.raw(m.timestamp, pitch);
                    outputs.normalized(m.timestamp, 0.);
                    break;
                  }
                  case Value: {
                    outputs.raw(m.timestamp, 0);
                    outputs.normalized(m.timestamp, 0.);
                    break;
                  }
                }
                push_poly_with_note_offs(m.timestamp, pitch);
              }
              else
              {
                push_poly(m.timestamp);
              }
            }
          }
          break;
        case Type::AfterTouch:
          if(t == libremidi::message_type::AFTERTOUCH)
          {
            outputs.midi.push_back(m);
            outputs.raw(m.timestamp, m.bytes[1]);
            outputs.normalized(m.timestamp, m.bytes[1] / 127.);
          }
          break;
        case Type::CC:
          if(t == libremidi::message_type::CONTROL_CHANGE)
          {
            if(inputs.index == 0 || inputs.index == m.bytes[1])
            {
              outputs.midi.push_back(m);
              output_2_bytes(m);
            }
          }
          break;
        case Type::PolyPressure:
          if(t == libremidi::message_type::POLY_PRESSURE)
          {
            if(inputs.index == 0 || inputs.index == m.bytes[1])
            {
              outputs.midi.push_back(m);
              output_2_bytes(m);
            }
          }
          break;
        case Type::PitchBend:
          if(t == libremidi::message_type::PITCH_BEND)
          {
            outputs.midi.push_back(m);
            const int pb = m.bytes[2] * 128 + m.bytes[1];
            outputs.raw(m.timestamp, pb);
            outputs.normalized(m.timestamp, pb / (128. * 128.) - 0.5);
          }
          break;
      }
    }
  }
};
}
