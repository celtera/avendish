#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/ChannelAllocator.hpp>
#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <Advanced/MidiScaler/MidiCommon/NoteTracker.hpp>
#include <halp/audio.hpp>
#include <halp/channel_mask.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>
#include <rnd/random.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>

namespace mtk
{
/**
 * @brief Spreads incoming notes over a set of MIDI channels, to play a
 * polyphonic part on several monophonic synths or to split a chord between
 * instruments.
 *
 * Note-offs follow their note's channel. In the chord modes, notes are held
 * back until no new note came for the grace period, then the whole chord is
 * dispatched at once, ordered by pitch.
 */
struct MidiDispatcher
{
  halp_meta(name, "Midi dispatcher")
  halp_meta(c_name, "midi_dispatcher")
  halp_meta(category, "Midi/Routing")
  halp_meta(author, "ossia score")
  halp_meta(description, "Spread incoming notes over a set of MIDI channels")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-dispatcher.html")
  halp_meta(uuid, "f8d568dd-1dd4-4aad-9b49-03628f874557")

  static constexpr int max_chord = 32;

  struct algorithm_control
  {
    static clang_buggy_consteval auto name() { return std::string_view{"Algorithm"}; }
    halp_meta(
        description,
        "Round robin: next channel of the set. Random: any free channel. "
        "Ping pong: back and forth over the set. Chord modes: the notes of a "
        "chord go to the channels of the set in order, in the order they were "
        "played, from the lowest note, or from the highest note.")
    enum enum_type
    {
      round_robin,
      random,
      ping_pong,
      chord,
      chord_ascending,
      chord_descending
    } value{round_robin};
    enum widget
    {
      combobox
    };
    struct range
    {
      std::string_view values[6]{"Round robin",     "Random", "Ping pong", "Chord",
                                 "Chord ascending", "Chord descending"};
      enum_type init = round_robin;
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
    halp::midi_bus<"MIDI", libremidi::message> midi;

    algorithm_control algorithm;

    struct : halp::channel_mask<"Channels", 0x000F>
    {
      halp_meta(description, "Channels the notes are spread over")
    } channels;

    struct : halp::spinbox_i32<"Surplus channel", halp::range{0, 16, 0}>
    {
      halp_meta(
          description,
          "Channel for the notes that find no free channel. 0: they take the "
          "channel released longest ago, or else end the oldest note (in the "
          "chord modes, the note on the channel of the same rank)")
    } surplus;

    struct : halp::hslider_f32<"Grace (ms)", halp::range{0., 200., 20.}>
    {
      halp_meta(
          description,
          "Chord modes: time in ms without a new note after which the chord is "
          "complete and dispatched")
    } grace;

    struct : halp::hslider_f32<"Release reserve (ms)", halp::range{0., 1000., 0.}>
    {
      halp_meta(
          description,
          "Time in ms during which a channel stays reserved after its note ends, "
          "for the release of the previous note")
    } reserve;

    struct : halp::spinbox_i32<"Seed", halp::range{0, 10000, 0}>
    {
      halp_meta(description, "Random mode: 0 is free running, other values repeat")
    } seed;

    struct : halp::toggle<"Copy to all">
    {
      halp_meta(
          description,
          "Copy controllers, pressure, pitch bend and program changes to every "
          "channel of the set and to the surplus channel, instead of passing "
          "them unchanged")
    } copy_to_all;
  } inputs;

  struct ui
  {
    halp_meta(name, "Midi dispatcher")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Spread")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::algorithm> algorithm;
      halp::item<&ins::channels> channels;
      halp::item<&ins::surplus> surplus;
      halp::item<&ins::copy_to_all> copy_to_all;
    } spread;

    struct
    {
      halp_meta(name, "Timing")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)

      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<
              &ins::algorithm, algorithm_control::chord, algorithm_control::chord_ascending,
              algorithm_control::chord_descending>{};
        }
        halp::item<&ins::grace> grace;
      } chord;

      halp::item<&ins::reserve> reserve;

      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::algorithm, algorithm_control::random>{};
        }
        halp::item<&ins::seed> seed;
      } random;
    } timing;
  };

  struct
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;
  } outputs;

  MidiDispatcher()
  {
    std::random_device rd;
    m_free_seed[0] = (uint64_t(rd()) << 32) ^ uint64_t(rd());
    m_free_seed[1] = (uint64_t(rd()) << 32) ^ uint64_t(rd());
    reseed();
  }

  void prepare(halp::setup info) noexcept
  {
    if(info.rate > 0.)
      m_rate = info.rate;
  }

  //! Sends a message, and frees the channel of each note-off it sends.
  auto emitter() noexcept
  {
    return [this](libremidi::message&& m) noexcept {
      if(is_note_off(m))
      {
        const int ch = channel_of(m);
        if(m_taken[ch - 1] > 0)
        {
          m_taken[ch - 1]--;
          m_alloc.release(ch, m_now + m.timestamp);
        }
      }
      outputs.midi.push_back(std::move(m));
    };
  }

  void stop() noexcept { m_stop = true; }

  using tick = halp::tick;
  void operator()(halp::tick t) noexcept
  {
    m_frames = t.frames > 0 ? t.frames : 0;

    if(m_stop)
    {
      m_chord_size = 0;
      m_notes.end_all(0, emitter());
      m_alloc.reset();
      m_taken = {};
      reseed();
      m_stop = false;
    }

    configure();

    for(const auto& in : inputs.midi)
    {
      const int64_t ts = m_frames > 0 ? std::clamp<int64_t>(in.timestamp, 0, m_frames - 1) : 0;
      if(m_chord_size > 0 && m_deadline < m_now + ts)
        dispatch_chord(m_deadline);
      handle(in, ts);
    }

    if(m_chord_size > 0 && m_deadline < m_now + m_frames)
      dispatch_chord(m_deadline);

    m_now += m_frames;
  }

private:
  struct pending_note
  {
    uint8_t channel{}, pitch{}, velocity{};
  };

  bool chord_mode() const noexcept
  {
    return m_algorithm == algorithm_control::chord
           || m_algorithm == algorithm_control::chord_ascending
           || m_algorithm == algorithm_control::chord_descending;
  }

  void configure() noexcept
  {
    m_alloc.set_channels(inputs.channels.mask());

    const auto algo = inputs.algorithm.value;
    if(algo != m_algorithm)
    {
      // A chord being collected goes out with the settings it was played with.
      if(m_chord_size > 0)
        dispatch_chord(m_now);
      m_algorithm = algo;
    }
    switch(m_algorithm)
    {
      case algorithm_control::round_robin:
        m_alloc.set_mode(channel_allocator::mode::round_robin);
        break;
      case algorithm_control::random:
        m_alloc.set_mode(channel_allocator::mode::random);
        break;
      case algorithm_control::ping_pong:
        m_alloc.set_mode(channel_allocator::mode::ping_pong);
        break;
      default:
        m_alloc.set_mode(channel_allocator::mode::lowest);
        break;
    }

    m_alloc.set_release_reserve(
        int64_t(std::llround(std::clamp(double(inputs.reserve.value), 0., 1000.) * m_rate / 1000.)));
    m_grace = int64_t(
        std::llround(std::clamp(double(inputs.grace.value), 0., 200.) * m_rate / 1000.));
    m_surplus = std::clamp(int(inputs.surplus.value), 0, 16);

    if(inputs.seed.value != m_seed)
    {
      m_seed = inputs.seed.value;
      reseed();
    }
  }

  void reseed() noexcept
  {
    if(m_seed != 0)
      m_rng.seed(uint64_t(m_seed), UINT64_C(0x9e3779b97f4a7c15));
    else
      m_rng.seed(m_free_seed[0], m_free_seed[1]);
  }

  double random01() noexcept { return double(m_rng()) / 4294967296.0; }

  int64_t offset_of(int64_t at) const noexcept
  {
    return m_frames > 0 ? std::clamp<int64_t>(at - m_now, 0, m_frames - 1) : 0;
  }

  void handle(const libremidi::message& m, int64_t ts) noexcept
  {
    auto emit = emitter();
    switch(classify(m))
    {
      case midi_kind::note_on: {
        if(chord_mode())
        {
          if(m_chord_size == max_chord)
            dispatch_chord(m_now + ts);
          m_chord[m_chord_size++]
              = {uint8_t(channel_of(m)), m.bytes[1], m.bytes[2]};
          m_deadline = m_now + ts + m_grace;
        }
        else
        {
          auto ch = m_alloc.allocate(m_now + ts, [this] { return random01(); });
          start_note(channel_of(m), m.bytes[1], m.bytes[2], ch, ts);
        }
        break;
      }

      case midi_kind::note_off: {
        const int ch = channel_of(m);
        if(m_chord_size > 0)
        {
          // The chord this note belongs to is complete.
          for(int i = 0; i < m_chord_size; i++)
          {
            if(m_chord[i].channel == ch && m_chord[i].pitch == m.bytes[1])
            {
              dispatch_chord(m_now + ts);
              break;
            }
          }
        }
        if(!m_notes.end(ch, m.bytes[1], ts, m.bytes[2], emit))
        {
          auto msg = m;
          msg.timestamp = ts;
          emit(std::move(msg));
        }
        break;
      }

      case midi_kind::control_change:
      case midi_kind::program_change:
      case midi_kind::pitch_bend:
      case midi_kind::channel_pressure:
      case midi_kind::poly_pressure: {
        if(is_all_notes_off(m))
        {
          m_chord_size = 0;
          m_notes.end_all(ts, emit);
        }

        if(inputs.copy_to_all.value && classify(m) != midi_kind::poly_pressure)
        {
          uint16_t targets = m_alloc.channels();
          if(m_surplus > 0)
            targets |= uint16_t(1u << (m_surplus - 1));
          for(int ch = 1; ch <= 16; ch++)
          {
            if((targets >> (ch - 1)) & 1)
            {
              auto msg = m;
              set_channel(msg, ch);
              msg.timestamp = ts;
              emit(std::move(msg));
            }
          }
        }
        else if(classify(m) == midi_kind::poly_pressure && m.size() >= 3)
        {
          // Follows the note it applies to when that note is held.
          auto msg = m;
          msg.timestamp = ts;
          for(int i = 0; i < m_notes.size(); i++)
          {
            const auto& h = m_notes[i];
            if(h.channel == channel_of(m) && h.pitch == m.bytes[1] && h.count > 0)
            {
              set_channel(msg, h.outputs[0].channel);
              break;
            }
          }
          emit(std::move(msg));
        }
        else
        {
          auto msg = m;
          msg.timestamp = ts;
          emit(std::move(msg));
        }
        break;
      }

      default: {
        auto msg = m;
        msg.timestamp = ts;
        emit(std::move(msg));
        break;
      }
    }
  }

  //! A channel for a note that found none free, when there is no surplus
  //! channel: the one released longest ago, or else the channel of a sounding
  //! note, which is ended. The stolen input note stays tracked without output,
  //! so that its own note-off is swallowed.
  std::optional<int> steal(int64_t ts, int rank) noexcept
  {
    if(auto ch = m_alloc.oldest_released())
    {
      m_alloc.take(*ch);
      return ch;
    }

    std::optional<int> target = rank >= 0 ? m_alloc.nth_usable(rank) : std::nullopt;
    int victim = -1;
    for(int i = 0; i < m_notes.size(); i++)
    {
      const auto& h = m_notes[i];
      if(h.count == 0)
        continue;
      if(!target || h.outputs[0].channel == *target)
      {
        victim = i;
        break;
      }
    }
    if(victim < 0)
    {
      // The channel of that rank holds no note: fall back to the oldest one.
      for(int i = 0; i < m_notes.size() && victim < 0; i++)
        if(m_notes[i].count > 0)
          victim = i;
      if(victim < 0)
        return std::nullopt;
    }

    const int ch = m_notes[victim].outputs[0].channel;
    const int in_ch = m_notes[victim].channel;
    const int in_pitch = m_notes[victim].pitch;
    auto emit = emitter();
    m_notes.end(in_ch, in_pitch, ts, 0, emit);
    m_notes.begin(in_ch, in_pitch, ts, emit);
    m_alloc.take(ch);
    return ch;
  }

  void start_note(int in_ch, int pitch, int vel, std::optional<int> ch, int64_t ts, int rank = -1) noexcept
  {
    auto emit = emitter();
    if(!ch && m_surplus == 0)
      ch = steal(ts, rank);

    // A note with no channel is still tracked, without output, so that its
    // note-off is swallowed too.
    auto& h = m_notes.begin(in_ch, pitch, ts, emit);
    if(!ch)
    {
      if(m_surplus == 0)
        return;
      if(m_notes.sounding(m_surplus, pitch) == 0)
      {
        m_alloc.take(m_surplus);
        m_taken[m_surplus - 1]++;
      }
      m_notes.play(h, m_surplus, pitch, vel, ts, emit);
      return;
    }
    m_taken[*ch - 1]++;
    m_notes.play(h, *ch, pitch, vel, ts, emit);
  }

  void dispatch_chord(int64_t at) noexcept
  {
    const int64_t ts = offset_of(at);
    const int n = m_chord_size;
    m_chord_size = 0;

    auto* first = m_chord.data();
    if(m_algorithm == algorithm_control::chord_ascending)
      std::stable_sort(first, first + n, [](const auto& a, const auto& b) {
        return a.pitch < b.pitch;
      });
    else if(m_algorithm == algorithm_control::chord_descending)
      std::stable_sort(first, first + n, [](const auto& a, const auto& b) {
        return a.pitch > b.pitch;
      });

    for(int i = 0; i < n; i++)
    {
      auto ch = m_alloc.allocate(m_now + ts);
      start_note(m_chord[i].channel, m_chord[i].pitch, m_chord[i].velocity, ch, ts, i);
    }
  }

  note_tracker<128, 1> m_notes;
  channel_allocator m_alloc;
  //! Note-ons sent on each channel and not yet ended.
  std::array<int, 16> m_taken{};

  std::array<pending_note, max_chord> m_chord{};
  int m_chord_size{};
  int64_t m_deadline{};

  rnd::pcg m_rng;
  uint64_t m_free_seed[2]{};
  int m_seed{0};

  algorithm_control::enum_type m_algorithm{algorithm_control::round_robin};
  int m_surplus{};
  int64_t m_grace{};
  double m_rate{48000.};
  int64_t m_now{};
  int m_frames{};
  bool m_stop{false};
};
}
