#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "StrStream.hpp"
#include "Tunings.h"
#include "TuningsImpl.h"

#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/midifile_port.hpp>
#include <libremidi/message.hpp>

#include <QDebug>

#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <sstream>

namespace mtk
{
/**
 * This example demonstrates loading and playing with MIDI data and scala files.
 *
 * It uses the very neat Surge library for handling tunings.
 */
struct MidiScaler
{
  halp_meta(name, "Midi scaler")
  halp_meta(c_name, "avnd_helpers_midifile_scaled")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-file-scaler.html")
  halp_meta(uuid, "c43300b0-6c98-4a38-81f2-b549f0297e38")
  halp_meta(category, "Midi")

  struct
  {
    // This port will read a midi file
    halp::midi_bus<"MIDI messages"> midi;

    // This port will read any kind of file
    struct : halp::file_port<"SCL file">
    {
      // Hosts can use this to only show a specific kind of file in a
      // file browser
      halp_meta(extensions, "*.scl");

      void update(MidiScaler& self)
      {
        if(this->file.bytes.empty())
        {
          self.scale = {};
        }
        else
        {
          try
          {
            compat::istrstream s(
                (const char*)this->file.bytes.data(),
                (std::streamsize)this->file.bytes.size());
            self.scale = Tunings::readSCLStream(s);
          }
          catch(...)
          {
          }
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

      void update(MidiScaler& self)
      {
        if(this->file.bytes.empty())
        {
          self.mapping = {};
        }
        else
        {
          try
          {
            compat::istrstream s(
                (const char*)this->file.bytes.data(),
                (std::streamsize)this->file.bytes.size());
            self.mapping = Tunings::readKBMStream(s);
          }
          catch(...)
          {
          }
        }
        self.update_tuning();
      }
    } kbm;

    halp::hslider_f32<"Frequency adjust", halp::range{-500., 500., 0.}> adjust;
    halp::hslider_f32<"Frequency scale", halp::range{0.5, 2., 1.}> rescale;
  } inputs;

  struct
  {
    struct
    {
      halp_meta(name, "Frequency output");
      std::vector<float> value{};
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
    }
    catch(...)
    {
      // Revert to the most basic tuning
      this->tuning = Tunings::Tuning{};
    }
  }

  std::optional<Tunings::Scale> scale;
  std::optional<Tunings::KeyboardMapping> mapping;
  Tunings::Tuning tuning;

  std::array<int, 128> active = {};

  void prepare(halp::setup s) { update_tuning(); }

  void operator()(int N)
  {
    outputs.freq.value.clear();

    // Update the active notes
    for(auto& msg : this->inputs.midi)
    {
      libremidi::message m;
      m.bytes.assign(msg.bytes.begin(), msg.bytes.end());

      switch(m.get_message_type())
      {
        case libremidi::message_type::NOTE_ON:
          this->active[m.bytes[1]] = m.bytes[2];
          break;
        case libremidi::message_type::NOTE_OFF:
          this->active[m.bytes[1]] = 0;
          break;
        default:
          break;
      }
    }

    for(std::size_t note = 0; note < this->active.size(); note++)
    {
      if(this->active[note] > 0)
      {
        float f = (tuning.frequencyForMidiNote(note) + inputs.adjust) * inputs.rescale;
        outputs.freq.value.push_back(f);
      }
    }
  }
};
}
