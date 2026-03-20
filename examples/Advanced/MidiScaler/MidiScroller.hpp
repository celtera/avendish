#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/container/flat_set.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/icl/split_interval_map.hpp>
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
 * This example demonstrates scrolling through the notes of a midi file
 * through manual user control
 */
struct MidiScroller
{
  halp_meta(name, "Midi file scroller")
  halp_meta(c_name, "avnd_helpers_scroller")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-file-scroller.html")
  halp_meta(uuid, "6a1429f8-f466-4748-ad1d-4fdd644e2359")
  halp_meta(category, "Midi")

  struct
  {
    struct : halp::midifile_port<"MIDI file", halp::simple_midi_track_event>
    {
      halp_flag(file_watch);
      void update(MidiScroller& s) { s.update_midi(); }
    } midi;
    struct : halp::spinbox_i32<"MIDI track", halp::range{1, 127, 1}>
    {
      void update(MidiScroller& s) { s.update_midi(); }
    } track;
    halp::hslider_f32<"Position"> position;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI messages"> midi;
  } outputs;

  using note_set = boost::container::small_flat_set<std::pair<uint8_t, uint8_t>, 4>;
  boost::icl::split_interval_map<int64_t, note_set> times;
  note_set current_notes;

  void update_midi()
  {
    struct note
    {
      int velocity;
      int64_t start;
    };

    times.clear();
    std::array<note, 128> notes;

    const int tracks = inputs.midi.midifile.tracks.size();
    if(tracks == 0)
      return;
    int track = inputs.track.value;

    if(track < 0)
      track = 0;
    if(track >= tracks)
      track = tracks - 1;

    for(const auto& ev : inputs.midi.midifile.tracks[track])
    {
      libremidi::message msg;
      msg.bytes = {ev.bytes[0], ev.bytes[1], ev.bytes[2]};

      uint8_t pitch = msg.bytes[1];
      uint8_t vel = msg.bytes[2];
      switch(msg.get_message_type())
      {
        case libremidi::message_type::NOTE_ON:
          notes[pitch] = {.velocity = vel, .start = ev.tick_absolute};
          break;
        case libremidi::message_type::NOTE_OFF: {
          auto end = ev.tick_absolute;
          auto vel = notes[pitch].velocity;
          if(vel > 0)
          {
            auto itv = boost::icl::interval<int64_t>::closed(notes[pitch].start, end);
            this->times.add(std::make_pair(itv, note_set{{pitch, vel}}));
          }
          notes[pitch].velocity = 0;
          break;
        }
        default:
          break;
      }
    }
  }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical t)
  {
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

    const int64_t target_tick = inputs.position.value * length;
    auto it = this->times.find(target_tick);
    if(it == this->times.end())
    {
      // notes off for everyone
      for(auto [pitch, vel] : current_notes)
      {
        auto m = libremidi::channel_events::note_off(1, pitch, vel);
        outputs.midi.midi_messages.emplace_back(
            halp::midi_msg{{m.bytes[0], m.bytes[1], m.bytes[2]}, 0});
      }
      current_notes.clear();
    }
    else
    {
      const note_set& new_notes = it->second;
      if(current_notes == new_notes)
        return;

      for(auto [pitch, vel] : current_notes)
      {
        if(!new_notes.contains({pitch, vel}))
        {
          auto m = libremidi::channel_events::note_off(1, pitch, vel);
          outputs.midi.midi_messages.emplace_back(
              halp::midi_msg{{m.bytes[0], m.bytes[1], m.bytes[2]}, 0});
        }
      }
      for(auto [pitch, vel] : new_notes)
      {
        if(!current_notes.contains({pitch, vel}))
        {
          auto m = libremidi::channel_events::note_on(1, pitch, vel);
          outputs.midi.midi_messages.emplace_back(
              halp::midi_msg{{m.bytes[0], m.bytes[1], m.bytes[2]}, 0});
        }
      }
      current_notes = new_notes;
    }
  }
};
}
