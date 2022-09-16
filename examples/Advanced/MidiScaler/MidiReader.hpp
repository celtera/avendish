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
 * This example demonstrates simple MIDI file playback
 */
struct MidiFileReader
{
  halp_meta(name, "Midi file reader")
  halp_meta(c_name, "avnd_helpers_midifile")
  halp_meta(uuid, "f2972e8f-17c3-40bb-919e-2663fe6fb3a5")
  halp_meta(category, "Midi")

  struct
  {
    struct : halp::midifile_port<"MIDI file", halp::simple_midi_track_event> {
      void update(MidiFileReader& m) { m.must_clear_notes = true; }
    } midi;
    struct : halp::spinbox_i32<"MIDI track", halp::range{1, 127, 1}> {
      void update(MidiFileReader& m) { m.must_clear_notes = true; }
    } track;
  } inputs;

  struct {
    halp::midi_bus<"MIDI messages"> midi;
  } outputs;

  /*

  //std::vector<halp::midi_bus<"Channel", halp::simple_midi_track_event>> midi_busses;
  std::vector<halp::hslider_f32<"Foo">> outputs;

  using inputs_t = decltype(inputs);
  using outputs_t = decltype(outputs);
  using allocate_fun = void(inputs_t&, outputs_t&);
  using replace_fun = void(MidiFileReader&, MidiFileReader&);
  using reconfigure_fun = void(std::function<allocate_fun>, std::function<replace_fun>);
  std::function<reconfigure_fun> reconfigure;
  */

  using channel_notes = boost::container::small_vector<uint8_t, 8>;
  std::array<channel_notes, 16> played_notes;
  bool must_clear_notes{};

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical t)
  {
    if(must_clear_notes)
    {
      for(int i = 0; i < 16; i++)
      {
        auto& c = played_notes[i];
        for(uint8_t note : c)
        {
          auto ev = libremidi::message::note_off(i + 1, note, 0);
          outputs.midi.midi_messages.emplace_back(halp::midi_msg{{ev.bytes[0], ev.bytes[1], ev.bytes[2]}, 0});
        }
      }
    }
    const auto length = this->inputs.midi.midifile.length;
    if(length == 0)
      return;

    const int track_index = inputs.track - 1;
    auto& i = inputs.midi.midifile.tracks;
    if(std::ssize(i) < track_index)
      return;

    auto& track = i[track_index];
    if(track.empty())
      return;

    const double pos_start = t.start_position_in_quarters * this->inputs.midi.midifile.ticks_per_beat;
    const double pos_end = t.end_position_in_quarters * this->inputs.midi.midifile.ticks_per_beat;
    const int64_t start = std::floor(pos_start);
    const int64_t end = std::floor(pos_end);

    auto start_it = std::lower_bound(
        track.begin(), track.end(),
        halp::simple_midi_track_event{.bytes = {}, .tick_absolute = start},
        [](const halp::simple_midi_track_event& lhs,
           const halp::simple_midi_track_event& rhs) {
      return lhs.tick_absolute < rhs.tick_absolute;
        });

    if(start_it == track.end())
      return;
    if(start_it->tick_absolute >= end)
      return;

    auto end_it = std::upper_bound(
          start_it, track.end(),
          halp::simple_midi_track_event{.bytes = {}, .tick_absolute = end},
          [](const halp::simple_midi_track_event& lhs,
          const halp::simple_midi_track_event& rhs) {
      return lhs.tick_absolute < rhs.tick_absolute;
    });

    for(auto it = start_it; it < end_it; ++it)
    {
      auto& ev = *it;
      double in = ev.tick_absolute;
      double x1 = pos_start;
      double x2 = pos_end;
      double y1 = 0;
      double y2 = t.frames - 1.;

      double mapped{};
      if(t.frames > 0)
        mapped = y1 + ((y2 - y1) / (x2 - x1)) * (in - x1);
      else
        mapped = 0;

      int64_t sample = y2 > 0. ? std::clamp(mapped, y1, y2) : 0;

      libremidi::message m{ev.bytes[0], ev.bytes[1], ev.bytes[2]};
      if(m.get_message_type() == libremidi::message_type::NOTE_ON)
      {
        int c = m.get_channel() - 1;
        this->played_notes[c].push_back(ev.bytes[1]);
      }
      else if(m.get_message_type() == libremidi::message_type::NOTE_OFF)
      {
        int c = m.get_channel() - 1;
        auto& chan = this->played_notes[c];
        auto it = std::find(chan.begin(), chan.end(), ev.bytes[1]);
        if(it != chan.end())
          chan.erase(it);
      }
      outputs.midi.midi_messages.emplace_back(halp::midi_msg{{ev.bytes[0], ev.bytes[1], ev.bytes[2]}, sample});
    }
  }
};
}
