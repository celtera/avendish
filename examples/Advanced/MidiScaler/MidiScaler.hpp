#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "Tunings.h"
#include "TuningsImpl.h"

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/file_port.hpp>
#include <halp/midifile_port.hpp>
#include <libremidi/message.hpp>
#include <cmath>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <sstream>
#include <optional>
#include <strstream>
#include <QDebug>

namespace examples::helpers
{
/**
 * This example demonstrates loading and playing with MIDI data and scala files.
 *
 * It uses the very neat Surge library for handling tunings.
 */
struct MidiFileOctaver
{
  halp_meta(name, "MidiFile scaled playback")
  halp_meta(c_name, "avnd_helpers_midifile_scaled")
  halp_meta(uuid, "c43300b0-6c98-4a38-81f2-b549f0297e38")

  struct
  {
    // This port will read a midi file
    struct : halp::midifile_port<"MIDI file"> {
      void update(MidiFileOctaver& self) {
        // O(number of events in the midi file) so we only do it once there
        self.length = this->midifile.get_length();
      }
    } midi;

    // This port will read any kind of file
    struct : halp::file_port<"SCL file">
    {
      // Hosts can use this to only show a specific kind of file in a
      // file browser
      halp_meta(extensions, "*.scl");

      void update(MidiFileOctaver& self)
      {
        if(this->file.bytes.empty())
        {
          self.scale = {};
        }
        else
        {
          try {
            std::istrstream s{
                (char*)this->file.bytes.data()
              , (std::streamsize)this->file.bytes.size()};
            self.scale = Tunings::readSCLStream(s);
          } catch(...) { }
        }
        self.update_tuning();
      }
    } scala;

    // This port will read any kind of file
    struct : halp::file_port<"KBM file">
    {
      // Hosts can use this to only show a specific kind of file in a
      // file browser
      halp_meta(extensions, "*.kbm");

      void update(MidiFileOctaver& self)
      {
        if(this->file.bytes.empty())
        {
          self.mapping = {};
        }
        else
        {
          try {
            std::istrstream s{
                              (char*)this->file.bytes.data()
                              , (std::streamsize)this->file.bytes.size()};
            self.mapping = Tunings::readKBMStream(s);
          } catch(...) { }
        }
        self.update_tuning();
      }
    } kbm;

    halp::hslider_f32<"Frequency adjust", halp::range{-500., 500., 0.}> adjust;
    halp::hslider_f32<"Frequency scale", halp::range{0.5, 2., 1.}> rescale;
    halp::hslider_f32<"Position"> position;
  } inputs;

  struct
  {
    struct {
      halp_meta(name, "Frequency output");
      float value{};
    } freq;
  } outputs;

  void update_tuning()
  try
  {
    if(scale && mapping)
      this->tuning = Tunings::Tuning{*this->scale, *this->mapping};
    else if(scale && !mapping)
      this->tuning = Tunings::Tuning{*this->scale};
    else if(!scale && mapping)
      this->tuning = Tunings::Tuning{*this->mapping};
    else
      this->tuning = Tunings::Tuning{};

    this->tuning = this->tuning.withSkippedNotesInterpolated();
  }
  catch(...)
  {
    // Maybe the tuning & mapping did not fit together
    try
    {
      if(scale)
        this->tuning = Tunings::Tuning{*this->scale};
      else
        this->tuning = Tunings::Tuning{};

      this->tuning = this->tuning.withSkippedNotesInterpolated();
    } catch(...)
    {
      // Revert to the most basic tuning
      this->tuning = Tunings::Tuning{};
    }
  }

  std::optional<Tunings::Scale> scale;
  std::optional<Tunings::KeyboardMapping> mapping;
  Tunings::Tuning tuning;

  int64_t length = 0;

  void prepare(halp::setup s)
  {
    update_tuning();
  }

  void operator()(int N)
  {
    if(length == 0)
      return;

    auto& i = inputs.midi.midifile.tracks;
    if(i.empty())
      return;
    if(i[0].empty())
      return;

    auto& track = i[0];

    int64_t cur = 0;
    const int64_t target_tick = inputs.position.value * this->length;

    for(auto it = track.begin(); it != track.end(); ++it)
    {
      auto& ev = *it;
      cur += ev.tick;
      if(cur >= target_tick)
      {
        libremidi::message msg;
        msg.bytes = {ev.bytes.begin(), ev.bytes.end()};
        if(msg.get_message_type() != libremidi::message_type::NOTE_ON)
          continue;

        int note = msg.bytes[1];
        outputs.freq.value = (tuning.frequencyForMidiNote(note) + inputs.adjust) * inputs.rescale;
        break;
      }
    }
  }
};
}

