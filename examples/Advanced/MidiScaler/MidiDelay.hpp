#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/EventQueue.hpp>
#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace mtk
{
/**
 * @brief Echoes of the incoming notes.
 *
 * Each note-on starts a chain of echoes, Time apart. Echo k plays the source
 * pitch plus k times Pitch step, at the source velocity times Feedback^k. The
 * chain ends after Repeats echoes (unless Infinite), when the velocity falls
 * under Vel. floor, or when the pitch leaves 0 - 127.
 * An echo lasts as long as its source note, or Length with Fixed length.
 *
 * Dry/Wet follows Midihub: 0 plays the source only, 50 the source and the
 * echoes at full velocity, 100 the echoes only; in between, the other side's
 * velocity is scaled down.
 *
 * Echoes keep the source's channel. With MPE, the pitch bend and pressure that
 * the source channel receives while the note is held are echoed with it.
 *
 * The settings in effect when a note starts apply to its whole chain, so that
 * changing them never leaves an echo without its note-off. Up to 32 source
 * notes are remembered; a 33rd ends the chain of the oldest.
 * An echo of a pitch that still sounds ends it first, so that the output never
 * starts a note twice.
 */
struct MidiDelay
{
  halp_meta(name, "Midi delay")
  halp_meta(c_name, "midi_delay")
  halp_meta(category, "Midi/Timing")
  halp_meta(author, "ossia score")
  halp_meta(description, "Repeat the incoming notes as echoes, with feedback and transposition")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-delay.html")
  halp_meta(uuid, "5ba4e20c-8936-43ab-aee5-8ac4555e05c1")

  static constexpr int max_sources = 32;
  static constexpr int max_expression_echoes = 16;

  struct ins
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;

    struct : halp::time_chooser<"Time", halp::range{0.001, 4., 0.25}>
    {
      halp_meta(description, "Time between two echoes, free or synced to the tempo")
    } time;

    struct : halp::spinbox_i32<"Repeats", halp::irange{1, 64, 3}>
    {
      halp_meta(description, "Number of echoes of each note")
    } repeats;

    struct : halp::toggle<"Infinite">
    {
      halp_meta(description, "Echo until the velocity falls under the floor")
    } infinite;

    struct : halp::hslider_i32<"Feedback", halp::irange{0, 100, 70}>
    {
      halp_meta(description, "Velocity of each echo, in percent of the previous one")
    } feedback;

    struct : halp::spinbox_i32<"Vel. floor", halp::irange{1, 127, 1}>
    {
      halp_meta(description, "An echo quieter than this ends the chain")
    } floor;

    struct : halp::spinbox_i32<"Pitch step", halp::irange{-24, 24, 0}>
    {
      halp_meta(
          description, "Semitones added at each echo; the chain ends when a note "
                       "leaves the MIDI range")
    } pitch_step;

    struct : halp::hslider_i32<"Dry/Wet", halp::irange{0, 100, 50}>
    {
      halp_meta(
          description, "0: the source only. 50: the source and the echoes. 100: "
                       "the echoes only.")
    } mix;

    struct : halp::toggle<"Fixed length">
    {
      halp_meta(description, "Echoes last Length instead of the source note's length")
    } fixed;

    struct : halp::time_chooser<"Length", halp::range{0.001, 4., 0.125}>
    {
      halp_meta(description, "Length of the echoes with Fixed length")
    } length;

    struct : halp::toggle<"MPE">
    {
      halp_meta(
          description, "Echo the pitch bend and pressure of the source note's "
                       "channel with each echo")
    } mpe;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;
  } outputs;

  struct ui
  {
    halp_meta(name, "Midi delay")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Echo")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::time> time;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::infinite, false>{};
        }
        halp::item<&ins::repeats> repeats;
      } repeats;
      halp::item<&ins::infinite> infinite;
      halp::item<&ins::feedback> feedback;
      halp::item<&ins::floor> floor;
      halp::item<&ins::pitch_step> pitch_step;
    } echo;

    struct
    {
      halp_meta(name, "Mix")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::mix> mix;
      halp::item<&ins::mpe> mpe;
    } mix;

    struct
    {
      halp_meta(name, "Length")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::fixed> fixed;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::fixed, true>{};
        }
        halp::item<&ins::length> length;
      } length;
    } length;
  };

  void prepare(halp::setup s) noexcept
  {
    if(s.rate > 0.)
      m_rate = s.rate;
  }

  void stop() noexcept { m_stop_requested = true; }

  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& t) noexcept
  {
    m_frames = t.frames > 0 ? t.frames : 0;
    if(m_stop_requested)
    {
      m_stop_requested = false;
      end_all(0, 0);
    }

    for(const auto& m : inputs.midi)
    {
      const int64_t ts
          = m_frames > 0 ? std::clamp<int64_t>(m.timestamp, 0, m_frames - 1) : 0;
      advance(ts);
      handle(m, ts);
    }
    advance(m_frames);
    m_now += m_frames;
  }

  //! Source notes whose echoes are not all out yet.
  int active_sources() const noexcept
  {
    int n = 0;
    for(auto& s : m_sources)
      n += s.used;
    return n;
  }

private:
  struct source
  {
    bool used{};
    bool dry{};      //!< its note-on went out
    bool done{};     //!< no echo left to start
    uint8_t channel{}; //!< 1 - 16
    uint8_t pitch{};
    uint8_t velocity{};
    uint8_t off_velocity{};
    uint32_t serial{};
    int64_t on_at{};
    int64_t off_at{-1}; //!< -1 while the key is held
    int64_t delay{};
    int64_t fixed_length{-1}; //!< -1: as long as the source
    int repeats{};      //!< -1: infinite
    int floor{};
    int step{};
    double feedback{};
    double wet{};
    int next_on{1};  //!< next echo to start
    int next_off{1}; //!< next echo to end

    //! Velocity of echo k, or 0 if the chain has ended before it.
    int echo_velocity(int k) const noexcept
    {
      if(repeats >= 0 && k > repeats)
        return 0;
      const int p = pitch + k * step;
      if(p < 0 || p > 127)
        return 0;
      const int v = int(std::lround(velocity * wet * std::pow(feedback, k)));
      return v >= std::max(1, floor) ? std::min(v, 127) : 0;
    }
    int echo_pitch(int k) const noexcept { return pitch + k * step; }
    int64_t echo_on(int k) const noexcept { return on_at + k * delay; }
    int64_t echo_off(int k) const noexcept
    {
      return fixed_length >= 0 ? echo_on(k) + fixed_length : off_at + k * delay;
    }
    bool off_known() const noexcept { return fixed_length >= 0 || off_at >= 0; }
  };

  // ------------------------------------------------------------------ output

  //! Per channel and pitch, how many notes sound. A note-on over a sounding
  //! pitch re-attacks it; the pitch ends with the last of its notes.
  void send_on(int c, int p, int v, int64_t ts) noexcept
  {
    auto& n = m_sounding[(c - 1) & 15][p & 127];
    if(n > 0)
      push(libremidi::channel_events::note_off(c, p, 0), ts);
    if(n < 0xFFFF)
      n++;
    push(libremidi::channel_events::note_on(c, p, v), ts);
  }

  void send_off(int c, int p, int v, int64_t ts) noexcept
  {
    auto& n = m_sounding[(c - 1) & 15][p & 127];
    if(n == 0)
      return;
    if(--n == 0)
      push(libremidi::channel_events::note_off(c, p, v), ts);
  }

  void push(libremidi::message m, int64_t ts) noexcept
  {
    m.timestamp = ts;
    outputs.midi.push_back(std::move(m));
  }

  void send(libremidi::message&& m) noexcept
  {
    const int ts = int(m.timestamp);
    if(is_note_on(m))
      send_on(channel_of(m), m.bytes[1], m.bytes[2], ts);
    else if(is_note_off(m))
      send_off(channel_of(m), m.bytes[1], m.bytes[2], ts);
    else
      push(std::move(m), ts);
  }

  // --------------------------------------------------------------- schedule

  //! Queues the echoes starting or ending before `limit`, then sends what is
  //! due before `ts` in the tick.
  void advance(int64_t ts) noexcept
  {
    if(m_frames <= 0)
      return;
    const int64_t limit = m_now + ts;
    for(auto& s : m_sources)
    {
      if(!s.used)
        continue;
      while(!s.done)
      {
        const int k = s.next_on;
        const int v = s.echo_velocity(k);
        if(v == 0)
        {
          s.done = true;
          break;
        }
        if(s.echo_on(k) >= limit)
          break;
        m_queue.schedule(
            s.echo_on(k),
            libremidi::channel_events::note_on(s.channel, s.echo_pitch(k), v));
        s.next_on++;
      }
      if(s.off_known())
      {
        while(s.next_off < s.next_on && s.echo_off(s.next_off) < limit)
        {
          const int k = s.next_off++;
          m_queue.schedule(
              std::max(s.echo_off(k), s.echo_on(k)),
              libremidi::channel_events::note_off(
                  s.channel, s.echo_pitch(k), s.off_velocity));
        }
      }
      if(s.done && s.off_at >= 0 && s.next_off == s.next_on)
        s = {};
    }
    m_queue.release(m_now, int(ts), [this](libremidi::message&& m) { send(std::move(m)); });
  }

  int64_t frames_of(float seconds) const noexcept
  {
    return int64_t(std::llround(std::max(0.f, seconds) * m_rate));
  }

  // ----------------------------------------------------------------- input

  void handle(const libremidi::message& m, int64_t ts) noexcept
  {
    switch(classify(m))
    {
      case midi_kind::note_on:
        note_on(channel_of(m), m.bytes[1] & 127, m.bytes[2] & 127, ts);
        return;
      case midi_kind::note_off:
        if(m.size() >= 3)
        {
          const int vel = (m.bytes[0] & 0xF0) == 0x80 ? (m.bytes[2] & 127) : 0;
          note_off(channel_of(m), m.bytes[1] & 127, vel, ts);
          return;
        }
        break;
      case midi_kind::control_change:
        if(is_all_notes_off(m))
        {
          end_all(channel_of(m), ts);
          push(m, ts);
          return;
        }
        break;
      case midi_kind::pitch_bend:
      case midi_kind::channel_pressure:
      case midi_kind::poly_pressure:
        if(inputs.mpe.value)
          echo_expression(m, ts);
        break;
      default:
        break;
    }
    push(m, ts);
  }

  void note_on(int c, int p, int v, int64_t ts) noexcept
  {
    source* slot = nullptr;
    for(auto& s : m_sources)
      if(!s.used)
      {
        slot = &s;
        break;
      }
    if(!slot)
    {
      slot = &m_sources[0];
      for(auto& s : m_sources)
        if(s.serial < slot->serial)
          slot = &s;
      retire(*slot, ts);
    }

    const int mix = std::clamp(inputs.mix.value, 0, 100);
    const double dry = mix <= 50 ? 1. : (100 - mix) / 50.;
    const double wet = mix >= 50 ? 1. : mix / 50.;

    source& s = *slot;
    s = {};
    s.used = true;
    s.channel = uint8_t(c);
    s.pitch = uint8_t(p);
    s.velocity = uint8_t(v);
    s.serial = m_serial++;
    s.on_at = m_now + ts;
    s.delay = frames_of(inputs.time.value);
    s.fixed_length
        = inputs.fixed.value ? std::max<int64_t>(1, frames_of(inputs.length.value)) : -1;
    s.repeats = inputs.infinite.value ? -1 : std::max(0, inputs.repeats.value);
    s.floor = inputs.floor.value;
    s.step = inputs.pitch_step.value;
    s.feedback = std::clamp(inputs.feedback.value, 0, 100) / 100.;
    s.wet = wet;
    if(s.delay < 1)
      s.done = true;

    const int dv = int(std::lround(v * dry));
    if(dv >= 1)
    {
      s.dry = true;
      send_on(c, p, dv, ts);
    }
  }

  void note_off(int c, int p, int vel, int64_t ts) noexcept
  {
    // Every chain of the key that is still held: a key struck twice without
    // release ends with its one note-off
    for(auto& s : m_sources)
    {
      if(!s.used || s.off_at >= 0 || s.channel != c || s.pitch != p)
        continue;
      s.off_at = m_now + ts;
      s.off_velocity = uint8_t(vel);
      if(s.dry)
        send_off(c, p, vel, ts);
      if(s.done && s.next_off == s.next_on)
        s = {};
    }
  }

  //! The oldest chain makes room: what it started ends now.
  void retire(source& s, int64_t ts) noexcept
  {
    if(s.off_at < 0 && s.dry)
      send_off(s.channel, s.pitch, 0, ts);
    // advance() leaves nothing of a chain in the queue: its echoes started
    // and not ended yet are these
    for(int k = s.next_off; k < s.next_on; k++)
      send_off(s.channel, s.echo_pitch(k), 0, ts);
    s = {};
  }

  //! Pitch bend and pressure of a held note's channel, replayed with its
  //! echoes (the latest held note of the channel, for a poly pressure the one
  //! of that key).
  void echo_expression(const libremidi::message& m, int64_t ts) noexcept
  {
    const int c = channel_of(m);
    const bool poly = classify(m) == midi_kind::poly_pressure;
    const source* src = nullptr;
    for(auto& s : m_sources)
    {
      if(!s.used || s.off_at >= 0 || s.channel != c)
        continue;
      if(poly && s.pitch != m.bytes[1])
        continue;
      if(!src || s.serial > src->serial)
        src = &s;
    }
    if(!src)
      return;
    for(int k = 1; k <= max_expression_echoes && src->echo_velocity(k) > 0; k++)
    {
      auto e = m;
      if(poly)
        e.bytes[1] = uint8_t(src->echo_pitch(k));
      m_queue.schedule(m_now + ts + k * src->delay, e);
    }
  }

  //! Ends what sounds and forgets the chains, on one channel or (0) all.
  void end_all(int channel, int64_t ts) noexcept
  {
    for(int c = 1; c <= 16; c++)
    {
      if(channel != 0 && c != channel)
        continue;
      for(int p = 0; p < 128; p++)
      {
        auto& n = m_sounding[c - 1][p];
        if(n > 0)
        {
          n = 0;
          push(libremidi::channel_events::note_off(c, p, 0), ts);
        }
      }
    }
    for(auto& s : m_sources)
      if(channel == 0 || s.channel == channel)
        s = {};
    m_queue.remove_if(
        [&](const auto& e) { return channel == 0 || channel_of(e.msg) == channel; });
  }

  std::array<source, max_sources> m_sources{};
  std::array<std::array<uint16_t, 128>, 16> m_sounding{};
  event_queue<2048> m_queue;
  int64_t m_now{};
  double m_rate{48000.};
  uint32_t m_serial{};
  int m_frames{};
  bool m_stop_requested{};
};
}
