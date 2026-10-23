#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "StrStream.hpp"
#include "Tunings.h"
#include "TuningsImpl.h"

#include <Advanced/MidiScaler/MidiCommon/ChannelAllocator.hpp>
#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <Advanced/MidiScaler/MidiCommon/NoteTracker.hpp>

#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/midifile_port.hpp>
#include <libremidi/message.hpp>

#include <QDebug>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <sstream>
#include <vector>

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
  halp_meta(description, "Convert active MIDI notes to frequencies using Scala tuning and keyboard-mapping files, and play them retuned on a MIDI output.")

  struct output_mode_control
  {
    static clang_buggy_consteval auto name() { return std::string_view{"MIDI output"}; }
    halp_meta(
        description,
        "Pitch bend: each note gets a channel of its own and a pitch bend. "
        "MTS sysex: notes pass unchanged and the tuning of the 128 keys is sent "
        "as MIDI Tuning Standard messages whenever it changes.")
    enum enum_type
    {
      pitch_bend,
      mts_sysex
    } value{pitch_bend};
    enum widget
    {
      combobox
    };
    struct range
    {
      std::string_view values[2]{"Pitch bend", "MTS sysex"};
      enum_type init = pitch_bend;
    };
    operator enum_type&() noexcept { return value; }
    operator const enum_type&() const noexcept { return value; }
    auto& operator=(enum_type t) noexcept
    {
      value = t;
      return *this;
    }
  };

  struct ins
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

    output_mode_control output_mode;

    struct : halp::spinbox_i32<"Bend range", halp::range{1, 96, 2}>
    {
      halp_meta(
          description,
          "Pitch bend range of the receiving synth in semitones: 2 for most "
          "synths, 48 for MPE synths. Incoming pitch bend is read with the same range.")
    } bend_range;

    struct : halp::lineedit<"Channels", "1-16">
    {
      halp_meta(
          description,
          "Channels the notes are spread over, for example 2-16. "
          "Empty or invalid: all 16 channels.")
    } channels;

    struct : halp::toggle<"Always send bend">
    {
      halp_meta(
          description,
          "Send a pitch bend before every note, even when the channel already "
          "has the right one")
    } always_bend;

    struct : halp::spinbox_i32<"Input bend range", halp::range{1, 96, 2}>
    {
      halp_meta(
          description,
          "Range in semitones of the incoming pitch bend, e.g. 2 for a keyboard "
          "wheel; the output uses Bend range")
    } input_bend_range;
  } inputs;

  struct ui
  {
    halp_meta(name, "Midi scaler")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Tuning")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::scala> scala;
      halp::item<&ins::kbm> kbm;
      halp::item<&ins::adjust> adjust;
      halp::item<&ins::rescale> rescale;
    } tuning;

    struct
    {
      halp_meta(name, "MIDI output")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::output_mode> output_mode;

      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::output_mode, output_mode_control::pitch_bend>{};
        }
        halp::item<&ins::bend_range> bend_range;
        halp::item<&ins::channels> channels;
        halp::item<&ins::always_bend> always_bend;
        halp::item<&ins::input_bend_range> input_bend_range;
      } bend;
    } output;
  };

  struct
  {
    struct
    {
      halp_meta(name, "Frequency output");
      std::vector<float> value{};
    } freq;

    halp::midi_bus<"MIDI", libremidi::message> midi;
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
    m_tuning_changed = true;
  }
  catch(...)
  {
    m_tuning_changed = true;
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

  void stop() noexcept { m_stop = true; }

  void operator()(int N)
  {
    process_midi_output(N);

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
  //! Fractional MIDI note (69 = A 440) of a key with the current tuning and
  //! frequency controls, or nothing when the frequency is not positive.
  std::optional<double> target_pitch(int key) const noexcept
  {
    const double f = (tuning.frequencyForMidiNote(key) + double(inputs.adjust))
                     * double(inputs.rescale);
    if(!(f > 0.))
      return std::nullopt;
    return 12. * std::log2(f / Tunings::MIDI_0_FREQ);
  }

  static int bend_value(double semitones, int range) noexcept
  {
    const double v = 8192. + std::round(semitones / double(range) * 8192.);
    return int(std::clamp(v, 0., 16383.));
  }

  //! MIDI Tuning Standard, real-time single note tuning change:
  //! F0 7F <device> 08 02 <program> <count> [<key> <semitone> <msb> <lsb>]... F7.
  //! Four keys per message keep each message within libremidi's inline storage.
  static constexpr int mts_keys_per_message = 4;

  template <typename Emit>
  void send_mts(int64_t ts, Emit& emit) const noexcept
  {
    for(int first = 0; first < 128; first += mts_keys_per_message)
    {
      libremidi::message m;
      m.bytes = {0xF0, 0x7F, 0x7F, 0x08, 0x02, 0x00, uint8_t(mts_keys_per_message)};
      for(int key = first; key < first + mts_keys_per_message; key++)
      {
        const auto data = mts_key_data(key);
        m.bytes.push_back(uint8_t(key));
        m.bytes.push_back(data[0]);
        m.bytes.push_back(data[1]);
        m.bytes.push_back(data[2]);
      }
      m.bytes.push_back(0xF7);
      m.timestamp = ts;
      emit(std::move(m));
    }
  }

  //! Semitone, then the 14-bit fraction of a semitone, MSB first.
  //! 7F 7F 7F means "no change" and is sent for unplayable frequencies.
  std::array<uint8_t, 3> mts_key_data(int key) const noexcept
  {
    const auto t = target_pitch(key);
    if(!t)
      return {0x7F, 0x7F, 0x7F};
    const double p = std::clamp(*t, 0., 127. + 16382. / 16384.);
    int semi = int(std::floor(p));
    int frac = int(std::round((p - semi) * 16384.));
    if(frac >= 16384)
    {
      semi++;
      frac = 0;
    }
    if(semi > 127)
    {
      semi = 127;
      frac = 16382;
    }
    return {uint8_t(semi), uint8_t((frac >> 7) & 0x7F), uint8_t(frac & 0x7F)};
  }

  //! Bit n set: channel n + 1 is in the "Channels" control.
  uint16_t channel_mask() noexcept
  {
    const auto& list = m_channel_list.update(inputs.channels.value, 1, 16);
    uint16_t mask = 0;
    for(int ch = 1; ch <= 16; ch++)
      if(list.contains(ch))
        mask |= uint16_t(1u << (ch - 1));
    return mask == 0 ? uint16_t(0xFFFF) : mask;
  }

  void process_midi_output(int N) noexcept
  {
    const int frames = N > 0 ? N : 0;
    auto& out = outputs.midi;

    auto emit = [this, &out](libremidi::message&& m) noexcept {
      if(is_note_off(m))
      {
        const int ch = channel_of(m);
        if(m_taken[ch - 1] > 0)
        {
          m_taken[ch - 1]--;
          m_alloc.release(ch, m_now + m.timestamp);
        }
      }
      out.push_back(std::move(m));
    };

    if(m_stop)
    {
      m_notes.end_all(0, emit);
      m_alloc.reset();
      m_taken = {};
      m_global_bend = 0.;
      m_stop = false;
    }

    const auto mode = inputs.output_mode.value;
    const int range = std::clamp(int(inputs.bend_range.value), 1, 96);
    const uint16_t mask = channel_mask();
    const float adjust = inputs.adjust;
    const float rescale = inputs.rescale;
    const bool retuned = m_tuning_changed || adjust != m_adjust || rescale != m_rescale;

    if(mode != m_mode || range != m_range || mask != m_mask)
    {
      // Held notes were placed for the previous settings: they end here so
      // that each of them still gets its note-off on its own channel.
      m_notes.end_all(0, emit);
      m_mode = mode;
      m_range = range;
      m_mask = mask;
      m_alloc.set_channels(mask);
      m_mts_sent = false;
    }
    m_tuning_changed = false;
    m_adjust = adjust;
    m_rescale = rescale;

    if(mode == output_mode_control::mts_sysex && (retuned || !m_mts_sent))
    {
      send_mts(0, emit);
      m_mts_sent = true;
    }

    for(const auto& in : this->inputs.midi)
    {
      libremidi::message m;
      m.bytes.assign(in.bytes.begin(), in.bytes.end());
      m.timestamp = frames > 0 ? std::clamp<int64_t>(in.timestamp, 0, frames - 1) : 0;

      if(mode == output_mode_control::mts_sysex)
        handle_mts(m, emit);
      else
        handle_bend(m, emit);
    }

    m_now += frames;
  }

  template <typename Emit>
  void handle_mts(libremidi::message& m, Emit& emit) noexcept
  {
    const auto ts = m.timestamp;
    if(is_note_on(m))
    {
      auto& h = m_notes.begin(channel_of(m), m.bytes[1], ts, emit);
      m_notes.play(h, channel_of(m), m.bytes[1], m.bytes[2], ts, emit);
    }
    else if(is_note_off(m))
    {
      if(!m_notes.end(channel_of(m), m.bytes[1], ts, m.bytes[2], emit))
        emit(std::move(m));
    }
    else
    {
      if(is_all_notes_off(m))
        m_notes.end_all(ts, emit);
      emit(std::move(m));
    }
  }

  template <typename Emit>
  void handle_bend(libremidi::message& m, Emit& emit) noexcept
  {
    const auto ts = m.timestamp;
    switch(classify(m))
    {
      case midi_kind::note_on:
        start_bent_note(channel_of(m), m.bytes[1], m.bytes[2], ts, emit);
        break;

      case midi_kind::note_off:
        // A note-off without a note is dropped: its key and channel mean
        // nothing on the output.
        m_notes.end(channel_of(m), m.bytes[1], ts, m.bytes[2], emit);
        break;

      case midi_kind::pitch_bend: {
        if(m.size() < 3)
          break;
        const int v = m.bytes[1] | (m.bytes[2] << 7);
        m_global_bend = double(v - 8192) / 8192. * inputs.input_bend_range.value;
        for(int ch = 1; ch <= 16; ch++)
          if(m_taken[ch - 1] > 0)
            send_bend(ch, ts, true, emit);
        break;
      }

      case midi_kind::poly_pressure: {
        // Follows the note it applies to.
        for(int i = 0; i < m_notes.size(); i++)
        {
          const auto& h = m_notes[i];
          if(h.channel == channel_of(m) && h.pitch == m.bytes[1] && h.count > 0)
          {
            auto msg = libremidi::channel_events::poly_pressure(
                h.outputs[0].channel, h.outputs[0].pitch, m.bytes[2]);
            msg.timestamp = ts;
            emit(std::move(msg));
            break;
          }
        }
        break;
      }

      case midi_kind::control_change:
      case midi_kind::program_change:
      case midi_kind::channel_pressure:
        // Channel-wide messages reach every channel the notes may be on.
        if(is_all_notes_off(m))
          m_notes.end_all(ts, emit);
        for(int ch = 1; ch <= 16; ch++)
        {
          if(m_alloc.usable(ch))
          {
            auto msg = m;
            set_channel(msg, ch);
            emit(std::move(msg));
          }
        }
        break;

      default:
        emit(std::move(m));
        break;
    }
  }

  template <typename Emit>
  void send_bend(int ch, int64_t ts, bool always, Emit& emit) noexcept
  {
    const int v = bend_value(m_offset[ch - 1] + m_global_bend, m_range);
    if(!always && m_last_bend[ch - 1] == v)
      return;
    m_last_bend[ch - 1] = v;
    auto msg = libremidi::channel_events::pitch_bend(ch, v);
    msg.timestamp = ts;
    emit(std::move(msg));
  }

  template <typename Emit>
  void start_bent_note(int in_ch, int key, int vel, int64_t ts, Emit& emit) noexcept
  {
    const auto t = target_pitch(key);
    if(!t)
      return;

    // The nearest note to the bent pitch keeps the bend within range.
    const double bent = *t + m_global_bend;
    const int note = std::clamp(int(std::lround(bent)), 0, 127);
    const double offset = *t - note;
    if(std::abs(offset + m_global_bend) > m_range + 1e-9)
      return;

    const int64_t now = m_now + ts;
    std::optional<int> ch;
    for(int attempt = 0; attempt < 16 && !ch; attempt++)
    {
      ch = m_alloc.allocate(now);
      if(ch)
        break;

      // Every channel is busy: a channel whose notes already have this bend
      // can take the note too.
      for(int c = 1; c <= 16 && !ch; c++)
      {
        if(m_alloc.usable(c) && m_taken[c - 1] > 0
           && std::abs(m_offset[c - 1] - offset) < 1e-6 && m_notes.sounding(c, note) == 0)
        {
          m_alloc.take(c);
          ch = c;
        }
      }
      if(ch)
        break;

      // Otherwise the oldest note makes room.
      if(m_notes.size() == 0)
        break;
      const int old_ch = m_notes[0].channel;
      const int old_pitch = m_notes[0].pitch;
      m_notes.end(old_ch, old_pitch, ts, 0, emit);
    }
    if(!ch)
      return;

    m_taken[*ch - 1]++;
    m_offset[*ch - 1] = offset;
    send_bend(*ch, ts, inputs.always_bend.value, emit);

    auto& h = m_notes.begin(in_ch, key, ts, emit);
    m_notes.play(h, *ch, note, vel, ts, emit);
  }

  note_tracker<128, 1> m_notes;
  channel_allocator m_alloc;
  cached_range_list m_channel_list;

  //! Per output channel: tuning offset of its notes in semitones, last pitch
  //! bend sent (-1: unknown), and notes started on it.
  std::array<double, 16> m_offset{};
  std::array<int, 16> m_last_bend{-1, -1, -1, -1, -1, -1, -1, -1,
                                  -1, -1, -1, -1, -1, -1, -1, -1};
  std::array<int, 16> m_taken{};

  //! Incoming pitch bend, in semitones, added to every note.
  double m_global_bend{};

  int64_t m_now{};
  output_mode_control::enum_type m_mode{output_mode_control::pitch_bend};
  int m_range{2};
  uint16_t m_mask{0xFFFF};
  float m_adjust{0.f};
  float m_rescale{1.f};
  bool m_tuning_changed{true};
  bool m_mts_sent{false};
  bool m_stop{false};
};
}
