#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <ossia/detail/small_vector.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <Advanced/MidiScaler/MidiCommon/Rhythm.hpp>
#include <libremidi/message.hpp>
#include <rnd/random.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>

namespace mtk
{
/**
 * @brief Loosens the timing, velocity and length of incoming MIDI notes.
 *
 * Unlike the usual "humanize" function, the deviations are not white noise.
 * Hennig et al. (PLoS ONE 6(10) e26457, 2011) measured human rhythmic
 * performances and found their fluctuations to be long-range correlated,
 * with a power spectral density close to 1/f (they report beta in [0.5; 1.5]).
 * In a listening test, the same amount of deviation was preferred, and even
 * judged *more precise*, when it was 1/f-correlated rather than uncorrelated.
 *
 * The "Colour" control therefore selects the exponent of the noise:
 * 0 is white (what every other humanizer does), 1 is the 1/f fluctuation
 * measured in human playing, 2 is brownian drift (the pulse wanders).
 *
 * Note that the correlation is over *note index*, not over time: the series
 * analysed in the literature is the sequence of per-beat deviations, so the
 * generator advances once per note (or once per chord, see "Chord").
 */
struct MidiHumanizer
{
public:
  halp_meta(name, "Midi Humanize")
  halp_meta(c_name, "midi_humanize")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(description, "Loosen the timing, velocity and length of MIDI notes")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-humanize.html")
  halp_meta(uuid, "b9830e77-82fd-4d45-8b24-f78e5608eb21")

  //! Maximum number of events waiting to be released
  static constexpr int max_pending = 128;
  //! Maximum number of notes tracked between their note-on and note-off
  static constexpr int max_active = 64;

  //! What one Chance draw covers.
  enum ChanceUnit
  {
    Note,
    Chord,
    Period
  };

  struct ins
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;

    // ---- Timing ---------------------------------------------------------
    struct : halp::time_chooser<"Timing", halp::range{0., 0.5, 0.02}>
    {
      halp_meta(description, "Standard deviation of the timing deviation")
    } timing;

    struct : halp::knob_f32<"Colour", halp::range{0., 2., 1.}>
    {
      halp_meta(
          description, "Noise exponent: 0 = white, 1 = 1/f as measured in human "
                       "playing, 2 = brownian drift")
    } colour;

    struct : halp::knob_f32<"Early", halp::range{0., 1., 0.}>
    {
      halp_meta(
          description, "How much of the deviation may be early instead of late. "
                       "Above 0 the object delays everything by Early * 3 * Timing, "
                       "which is real added latency.")
    } early;

    // ---- Velocity -------------------------------------------------------
    struct : halp::hslider_i32<"Velocity", halp::range{0, 64, 8}>
    {
      halp_meta(description, "Standard deviation of the velocity deviation")
    } velocity;

    struct : halp::range_slider_f32<
                 "Vel. range", halp::range_slider_range{1., 127., {1., 127.}}>
    {
      halp_meta(description, "Output velocities are clamped into this range")
    } velocity_range;

    // ---- Length ---------------------------------------------------------
    struct : halp::knob_f32<"Length", halp::range{0., 1., 0.}>
    {
      halp_meta(
          description,
          "Bipolar random scaling of the note durations. Lengthening is always "
          "possible; shortening is bounded by the lookahead, since a note's "
          "duration is only known once its note-off arrives -- raise Early to "
          "shorten long notes.")
    } length;

    // ---- Chance ---------------------------------------------------------
    struct : halp::hslider_f32<"Chance", halp::range{0., 1., 1.}>
    {
      halp_meta(description, "Probability that a note is played at all")
    } chance;

    // ---- Chords ---------------------------------------------------------
    struct : halp::time_chooser<"Chord", halp::range{0., 0.1, 0.02}>
    {
      halp_meta(
          description,
          "Notes arriving within this window share a single timing deviation, "
          "so that chords are displaced as a whole instead of being smeared")
    } chord_window;

    struct : halp::time_chooser<"Spread", halp::range{0., 0.2, 0.}>
    {
      halp_meta(description, "Strum: delay between successive notes of a chord")
    } spread;

    // ---- Scope ----------------------------------------------------------
    struct : halp::spinbox_i32<"Channel", halp::range{0, 16, 0}>
    {
      halp_meta(description, "MIDI channel (0 = all)")
    } channel;

    struct : halp::range_slider_f32<
                 "Key range", halp::range_slider_range{0., 127., {0., 127.}}>
    {
      halp_meta(description, "Only notes in this range are humanized")
    } key_range;

    struct : halp::spinbox_i32<"Seed", halp::range{0, 10000, 0}>
    {
      halp_meta(
          description,
          "0 keeps running freely; any other value gives a reproducible take, "
          "restarted whenever the transport rewinds")
    } seed;

    // ---- Pitch ----------------------------------------------------------
    struct : halp::hslider_i32<"Pitch", halp::range{0, 12, 0}>
    {
      halp_meta(
          description,
          "Standard deviation, in semitones, of a random transposition of each "
          "note (its note-off follows it)")
    } pitch;

    // ---- Chance, extended -----------------------------------------------
    struct : halp::enum_t<ChanceUnit, "Chance per">
    {
      enum widget
      {
        combobox
      };
      halp_meta(
          description,
          "Note draws Chance for every note. Chord draws once for the notes "
          "arriving within the Chord window. Period draws once per period: a "
          "failed draw silences every note of that period.")
    } chance_unit;

    struct : halp::time_chooser<"Period", halp::range{0.01, 8., 0.5}>
    {
      halp_meta(description, "Length of a chance period, on the musical grid")
    } chance_period;

    struct : halp::toggle<"Vel. chance">
    {
      halp_meta(
          description,
          "The velocity is also a probability: 127 always plays, 64 plays about "
          "half of the time")
    } velocity_chance;

    // ---- Euclid gate ----------------------------------------------------
    struct : halp::toggle<"Euclid">
    {
      halp_meta(
          description, "Drops the notes arriving on a rest of a euclidean rhythm")
    } euclid;

    struct : halp::spinbox_i32<"Steps", halp::range{1, 64, 16}>
    {
      halp_meta(description, "Steps in the euclidean rhythm")
    } euclid_steps;

    struct : halp::spinbox_i32<"Pulses", halp::range{0, 64, 4}>
    {
      halp_meta(description, "Hits spread as evenly as possible over the steps")
    } euclid_pulses;

    struct : halp::spinbox_i32<"Rotation", halp::range{0, 63, 0}>
    {
      halp_meta(description, "Turns the rhythm this many steps later")
    } euclid_rotation;

    struct : halp::time_chooser<"Step", halp::range{0.01, 2., 0.125}>
    {
      halp_meta(
          description, "Length of a step, on the musical grid: the rhythm starts "
                       "at the start of the timeline")
    } euclid_step;

    // ---- Controllers ----------------------------------------------------
    struct : halp::spinbox_i32<"Random CC", halp::range{-1, 127, -1}>
    {
      halp_meta(
          description, "Controller whose values get a random offset (-1: none)")
    } random_cc;

    struct : halp::range_slider_f32<
                 "CC random range", halp::range_slider_range{-127., 127., {0., 0.}}>
    {
      halp_meta(
          description, "The offset added to that controller is drawn uniformly "
                       "in this range")
    } random_cc_range;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;
  } outputs;

  //! std::random_device can allocate, block and throw, so it is drawn once here
  //! -- at node construction -- and never from the processing thread. prepare()
  //! is not a safe place for it: it re-runs from inside the audio callback
  //! whenever the buffer size grows.
  MidiHumanizer()
  {
    std::random_device rd;
    m_free_seed[0] = (uint64_t(rd()) << 32) ^ uint64_t(rd());
    m_free_seed[1] = (uint64_t(rd()) << 32) ^ uint64_t(rd());
    reseed();
  }

  void prepare(halp::setup info) noexcept
  {
    const double rate = (info.rate > 0.) ? info.rate : m_rate;

    // prepare() re-runs from inside the audio callback whenever the buffer
    // grows, and this is not a context where note-offs can be emitted -- so it
    // must not drop the queue, or every sounding note hangs. Nothing scheduled
    // depends on the buffer size: the dates are in samples, and only an actual
    // sample-rate change invalidates them. (score rebuilds the whole graph on a
    // rate change, so that case starts from a fresh object anyway.)
    if(rate != m_rate)
    {
      m_rate = rate;
      full_reset();
    }
  }

  //! Called by the host when playback stops; the next tick, which may have no
  //! frames, ends every sounding note and drops the queue.
  void stop() noexcept { m_stop_requested = true; }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical t) noexcept
  {
    const int frames = t.frames > 0 ? t.frames : 0;

    cache_parameters();

    if(m_stop_requested)
    {
      m_stop_requested = false;
      flush_sounding(0);
      full_reset();
    }

    m_tempo = t.tempo > 0. ? t.tempo : 120.;
    m_quarter_start = t.start_position_in_quarters;
    m_quarters_per_frame
        = (frames > 0 && t.end_position_in_quarters > t.start_position_in_quarters)
              ? (t.end_position_in_quarters - t.start_position_in_quarters) / frames
              : 0.;

    // A rewind restarts a seeded take, so that rendering twice gives the same
    // result. Free-running (seed 0) just keeps going. Anything already sent out
    // has to be turned off first, or dropping the queue hangs it.
    if(m_seed != 0 && t.position_in_frames < m_last_position)
    {
      flush_sounding(0);
      full_reset();
    }
    m_last_position = t.position_in_frames;

    if(frames <= 0)
      return;

    for(const auto& m : inputs.midi)
      handle(m, m_now + std::clamp<int64_t>(m.timestamp, 0, frames));

    release(frames);

    m_now += frames;
  }

  struct ui
  {
    halp_meta(name, "Midi Humanize")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Timing")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)

      halp::item<&ins::timing> timing;

      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::item<&ins::colour> colour;
        halp::item<&ins::early> early;
      } shape;
    } timing;

    struct
    {
      halp_meta(name, "Notes")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)

      halp::item<&ins::velocity> velocity;
      halp::item<&ins::velocity_range> velocity_range;
      halp::item<&ins::length> length;
      halp::item<&ins::chance> chance;
      halp::item<&ins::pitch> pitch;
    } notes;

    struct
    {
      halp_meta(name, "Settings")
      halp_meta(layout, halp::layouts::tabs)
      halp_meta(background, halp::colors::background_darker)

      struct
      {
        halp_meta(name, "Chords")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::chord_window> chord_window;
        halp::item<&ins::spread> spread;
      } chords;

      struct
      {
        halp_meta(name, "Scope")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::channel> channel;
        halp::item<&ins::key_range> key_range;
        halp::item<&ins::seed> seed;
      } scope;

      struct
      {
        halp_meta(name, "Chance")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::chance_unit> chance_unit;
        struct
        {
          halp_meta(layout, halp::layouts::vbox)
          static constexpr auto condition()
          {
            return halp::enabled_when<&ins::chance_unit, Period>{};
          }
          halp::item<&ins::chance_period> chance_period;
        } period;
        halp::item<&ins::velocity_chance> velocity_chance;
      } chance;

      struct
      {
        halp_meta(name, "Gate")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::euclid> euclid;
        struct
        {
          halp_meta(layout, halp::layouts::vbox)
          static constexpr auto condition()
          {
            return halp::enabled_when<&ins::euclid, true>{};
          }
          halp::item<&ins::euclid_steps> euclid_steps;
          halp::item<&ins::euclid_pulses> euclid_pulses;
          halp::item<&ins::euclid_rotation> euclid_rotation;
          halp::item<&ins::euclid_step> euclid_step;
        } rhythm;
      } gate;

      struct
      {
        halp_meta(name, "CC")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::random_cc> random_cc;
        halp::item<&ins::random_cc_range> random_cc_range;
      } cc;
    } settings;
  };

private:
  // ------------------------------------------------------------- generators

  /**
   * @brief White noise fractionally integrated into 1/f^beta noise.
   *
   * y[n] = sum_k h[k] w[n-k], with h[0] = 1 and h[k] = h[k-1] * (d + k - 1) / k
   * for d = beta / 2. This is the impulse response of (1 - z^-1)^(-beta/2), so
   * the output spectrum is |1 - e^-jw|^-beta, i.e. proportional to 1/f^beta at
   * low frequencies. See Kasdin, "Discrete simulation of colored noise and
   * stochastic processes and 1/f^alpha power law noise generation",
   * Proc. IEEE 83(5), 1995.
   *
   * d = 0 gives h = {1, 0, 0, ...}, i.e. plain white noise; d = 1 gives
   * h = {1, 1, 1, ...}, i.e. a running sum, i.e. brownian motion.
   *
   * The response has to be truncated, and for beta near 2 a plain truncation is
   * a rectangular window: its spectral nulls punch holes in the result, which
   * is audible as whole drift periodicities being missing. So the coefficients
   * are damped by a leak instead, h[k] *= r^k, which makes the filter
   * (1 - r z^-1)^(-beta/2): the spectrum still follows 1/f^beta above the leak
   * frequency, rolls off smoothly below it, and is stationary for any beta.
   * That low-frequency flattening is wanted here anyway -- an exact brownian
   * process would let the pulse wander away without bound.
   */
  struct fractional_noise
  {
    static constexpr int taps = 256;
    //! exp(-4 / taps): the response has decayed by e^-4 when it is cut off
    static constexpr double leak = 0.98449644127;

    float h[taps]{};
    float w[taps]{};
    int pos{};
    float beta{-1.f};
    float norm{1.f};

    void set_beta(float b) noexcept
    {
      if(b == beta)
        return;
      beta = b;

      const double d = 0.5 * double(b);
      double acc = 1.0;
      double lk = 1.0;
      double sumsq = 1.0;
      h[0] = 1.f;
      for(int k = 1; k < taps; k++)
      {
        acc *= (d + k - 1) / double(k);
        lk *= leak;
        h[k] = float(acc * lk);
        sumsq += (acc * lk) * (acc * lk);
      }

      // Normalize to unit variance so that the amount controls keep meaning
      // the same thing when the colour changes.
      norm = float(1.0 / std::sqrt(sumsq));
    }

    void reset() noexcept
    {
      for(auto& v : w)
        v = 0.f;
      pos = 0;
    }

    float next(float white) noexcept
    {
      w[pos] = white;

      double y = 0.;
      int i = pos;
      for(int k = 0; k < taps; k++)
      {
        y += double(h[k]) * double(w[i]);
        if(--i < 0)
          i = taps - 1;
      }

      pos = (pos + 1 < taps) ? pos + 1 : 0;
      return float(y * double(norm));
    }
  };

  float uniform() noexcept
  {
    return float(double(m_rng()) / double(4294967295.0));
  }

  //! Box-Muller, so that a given seed always gives the same take regardless of
  //! the standard library's distribution implementation.
  float gaussian() noexcept
  {
    if(m_have_spare)
    {
      m_have_spare = false;
      return m_spare;
    }

    float u1 = uniform();
    if(u1 < 1e-7f)
      u1 = 1e-7f;
    const float u2 = uniform();

    const float mag = std::sqrt(-2.f * std::log(u1));
    const float ang = 2.f * std::numbers::pi_v<float> * u2;

    m_spare = mag * std::sin(ang);
    m_have_spare = true;
    return mag * std::cos(ang);
  }

  // ------------------------------------------------------------- scheduling

  struct scheduled
  {
    int64_t at{};
    libremidi::message msg;
    //! The note a note-on / note-off belongs to; 0 when untracked (a note
    //! outside the scope, or a note-off we never saw the note-on of).
    uint32_t note{};
  };

  struct active_note
  {
    int64_t arrival_on{}; //!< when the note-on arrived
    int64_t on_at{};      //!< when the note-on was scheduled
    double length_scale{1.};
    uint32_t id{};
    uint8_t channel{};
    uint8_t pitch{};     //!< as received: what the note-off is matched with
    uint8_t out_pitch{}; //!< as sent, after the Pitch transposition
    bool dropped{};
  };

  void schedule(int64_t at, const libremidi::message& m, uint32_t note = 0) noexcept
  {
    // Never emit in the past
    if(at < m_now)
      at = m_now;

    if(m_pending.size() >= max_pending)
    {
      // Saturated: drop rather than allocate on the audio thread. But dropping
      // a note-off hangs a note forever, while dropping anything else does not,
      // so a note-off evicts the furthest-away non-note-off instead.
      if(!is_note_off_msg(m))
        return;

      auto victim = m_pending.end();
      for(auto it = m_pending.begin(); it != m_pending.end(); ++it)
        if(!is_note_off_msg(it->msg))
          victim = it;

      // Everything queued is already a note-off: something has to give either
      // way, so give up the furthest-away one -- the note that would hang is
      // then the one that had the longest still to run, and this one gets in.
      if(victim == m_pending.end())
        victim = std::prev(m_pending.end());

      m_pending.erase(victim);
    }

    const auto it = std::upper_bound(
        m_pending.begin(), m_pending.end(), at,
        [](int64_t v, const scheduled& s) { return v < s.at; });

    auto& e = *m_pending.insert(it, scheduled{at, m, note});
    e.msg.timestamp = 0;
  }

  static constexpr uint32_t untracked_sounding = UINT32_MAX;

  static bool is_note_on_msg(const libremidi::message& m) noexcept
  {
    return m.get_message_type() == libremidi::message_type::NOTE_ON && m.size() >= 3
           && m.bytes[2] != 0;
  }

  //! Sends a message, keeping the output well-formed: per channel and pitch,
  //! note-ons and note-offs alternate. The deviations can make a note of a
  //! pitch start before the previous one of that pitch has ended, or its
  //! note-off land after the next note-on: the previous note is then ended
  //! first, and a note-off for a note that is no longer the one sounding is
  //! dropped -- it would cut the next note, or be a stray note-off.
  void send(libremidi::message msg, uint32_t note, int64_t ts) noexcept
  {
    msg.timestamp = ts;
    const bool on = is_note_on_msg(msg);
    const bool off = !on && is_note_off_msg(msg);
    if(!on && !off)
    {
      outputs.midi.push_back(std::move(msg));
      return;
    }

    auto& sounding = m_sounding[(msg.get_channel() - 1) & 15][msg.bytes[1] & 127];
    if(on)
    {
      if(sounding != 0)
      {
        auto end = libremidi::channel_events::note_off(
            msg.get_channel(), msg.bytes[1], 0);
        end.timestamp = ts;
        outputs.midi.push_back(std::move(end));
      }
      sounding = note != 0 ? note : untracked_sounding;
      outputs.midi.push_back(std::move(msg));
    }
    else
    {
      if(sounding == 0)
        return;
      if(note != 0 && sounding != untracked_sounding && sounding != note)
        return;
      sounding = 0;
      outputs.midi.push_back(std::move(msg));
    }
  }

  void release(int frames) noexcept
  {
    const int64_t limit = m_now + frames;

    auto it = m_pending.begin();
    for(; it != m_pending.end() && it->at < limit; ++it)
      send(
          it->msg, it->note,
          int64_t(std::clamp<int64_t>(it->at - m_now, 0, frames - 1)));
    m_pending.erase(m_pending.begin(), it);
  }

  // ---------------------------------------------------------------- routing

  bool in_scope(const libremidi::message& m, int pitch) const noexcept
  {
    if(m_channel != 0 && m.get_channel() != m_channel)
      return false;
    return pitch >= m_key_lo && pitch <= m_key_hi;
  }

  void handle(const libremidi::message& m, int64_t arrival) noexcept
  {
    const auto type = m.get_message_type();

    switch(type)
    {
      case libremidi::message_type::NOTE_ON:
        if(m.size() >= 3)
        {
          if(m.bytes[2] == 0)
            note_off(m, arrival);
          else
            note_on(m, arrival);
          return;
        }
        break;

      case libremidi::message_type::NOTE_OFF:
        if(m.size() >= 3)
        {
          note_off(m, arrival);
          return;
        }
        break;

      case libremidi::message_type::CONTROL_CHANGE:
        // All sound off / all notes off. The CC goes out with no latency, so
        // anything still queued would escape after it -- including note-ons,
        // whose note-offs are no longer tracked. Turn everything off and drop
        // the queue instead of just forgetting the tracking table.
        if(m.size() >= 2 && (m.bytes[1] == 120 || m.bytes[1] == 123))
        {
          const int ts = int(std::max<int64_t>(0, arrival - m_now));
          flush_sounding(ts);
          m_pending.clear();
          m_active.clear();
          m_chord_open = false;

          auto msg = m;
          msg.timestamp = ts;
          outputs.midi.push_back(std::move(msg));
          return;
        }
        if(m.size() >= 3 && m_random_cc >= 0 && m.bytes[1] == m_random_cc
           && (m_channel == 0 || m.get_channel() == m_channel))
        {
          auto msg = m;
          msg.bytes[2] = uint8_t(random_cc_value(m.bytes[2]));
          schedule(arrival + m_latency, msg);
          return;
        }
        break;

      default:
        break;
    }

    // Everything else rides along with the same latency, so that controllers
    // stay aligned with the notes they belong to.
    schedule(arrival + m_latency, m);
  }

  //! One timing deviation per note, or per chord when several notes arrive
  //! close together. Returns the offset in samples.
  int64_t next_timing_offset(int64_t arrival) noexcept
  {
    const bool same_chord
        = m_chord_open && (arrival - m_chord_anchor) <= m_chord_window;

    if(!same_chord)
    {
      m_chord_anchor = arrival;
      m_chord_open = true;
      m_chord_index = 0;
      m_chord_offset = int64_t(std::llround(
          double(m_timing_samples) * double(m_timing_noise.next(gaussian()))));
    }
    else
    {
      ++m_chord_index;
    }

    return m_chord_offset + int64_t(m_chord_index) * m_spread_samples;
  }

  void note_on(const libremidi::message& m, int64_t arrival) noexcept
  {
    const int pitch = m.bytes[1];

    if(!in_scope(m, pitch))
    {
      schedule(arrival + m_latency, m);
      return;
    }

    // A previous instance of the same note stays tracked: note-offs match the
    // oldest one first, as MIDI does, and the output stage ends it if this
    // one starts before its note-off.
    const bool gated = m_euclid && !euclid_hit_at(arrival);
    const bool dropped = gated || !chance_passes(m, arrival);

    active_note note;
    note.id = m_next_id++;
    if(m_next_id == 0 || m_next_id == untracked_sounding)
      m_next_id = 1;
    note.arrival_on = arrival;
    note.channel = uint8_t(m.get_channel());
    note.pitch = uint8_t(pitch);
    note.dropped = dropped;

    // The generators are advanced even for dropped notes, so that switching
    // Chance around does not change the deviation sequence of the notes that
    // do play.
    const int64_t offset = next_timing_offset(arrival);
    const float vel_dev = m_velocity_noise.next(gaussian());
    const float len_dev = m_length_noise.next(gaussian());
    const float pitch_dev = m_pitch_noise.next(gaussian());
    note.out_pitch = uint8_t(std::clamp(
        pitch + int(std::lround(double(m_pitch) * double(pitch_dev))), 0, 127));

    note.on_at = std::max(arrival, arrival + m_latency + offset);
    note.length_scale
        = std::clamp(1. + double(m_length) * double(len_dev), 0.05, 4.);

    if(m_active.size() >= max_active)
    {
      // Leaving the note untracked is not an option: its note-off would be
      // scheduled without the note-on's timing offset and could overtake it,
      // hanging the note. Steal the oldest tracked note instead, turning it off
      // as we go.
      const active_note& victim = m_active.front();
      if(!victim.dropped)
      {
        auto off
            = libremidi::channel_events::note_off(victim.channel, victim.out_pitch, 0);
        schedule(std::max(m_now, victim.on_at + 1), off, victim.id);
      }
      m_active.erase(m_active.begin());
    }
    m_active.push_back(note);

    if(dropped)
      return;

    auto out = m;
    const int vel = int(std::lround(double(m.bytes[2]) + double(m_velocity) * double(vel_dev)));
    out.bytes[1] = note.out_pitch;
    out.bytes[2] = uint8_t(std::clamp(vel, m_vel_lo, m_vel_hi));
    schedule(note.on_at, out, note.id);
  }

  //! Position of a date of the current tick on the musical grid, in quarter
  //! notes. When the host gives no musical position, the frame clock and the
  //! tempo stand in for it.
  double quarter_at(int64_t arrival) const noexcept
  {
    if(m_quarters_per_frame > 0.)
      return m_quarter_start + double(arrival - m_now) * m_quarters_per_frame;
    return double(arrival) / m_rate * m_tempo / 60.;
  }

  //! Index of the grid cell of `length_s` seconds a date falls in. The small
  //! bias keeps a note sitting exactly on a boundary in the cell it starts.
  int64_t grid_index(int64_t arrival, double length_s) const noexcept
  {
    const double len_q = length_s * m_tempo / 60.;
    if(len_q <= 0.)
      return 0;
    return int64_t(std::floor(quarter_at(arrival) / len_q + 1e-6));
  }

  bool euclid_hit_at(int64_t arrival) const noexcept
  {
    const int64_t idx = grid_index(arrival, m_euclid_step_s);
    const int64_t step = ((idx % m_euclid_steps) + m_euclid_steps) % m_euclid_steps;
    return mtk::euclid_hit(int(step), m_euclid_steps, m_euclid_pulses, m_euclid_rotation);
  }

  //! Draws as little as possible: with Chance at 1 and the velocity not used
  //! as a probability, nothing is drawn, so the other generators keep their
  //! sequence.
  bool chance_passes(const libremidi::message& m, int64_t arrival) noexcept
  {
    const float vel_p = m_velocity_chance ? float(m.bytes[2]) / 127.f : 1.f;

    switch(m_chance_unit)
    {
      case Note:
      default: {
        const float p = m_chance * vel_p;
        return !((p < 1.f) && (uniform() > p));
      }

      case Chord: {
        const bool new_chord
            = !(m_chord_open && (arrival - m_chord_anchor) <= m_chord_window);
        if(new_chord)
          m_chord_draw = (m_chance < 1.f || m_velocity_chance) ? uniform() : 0.f;
        return !(m_chord_draw > m_chance * vel_p);
      }

      case Period: {
        const int64_t idx = grid_index(arrival, m_chance_period_s);
        if(!m_period_valid || idx != m_period_index)
        {
          m_period_valid = true;
          m_period_index = idx;
          m_period_open = !((m_chance < 1.f) && (uniform() > m_chance));
        }
        if(!m_period_open)
          return false;
        return !((vel_p < 1.f) && (uniform() > vel_p));
      }
    }
  }

  //! The random offset of the controller selected by Random CC.
  int random_cc_value(int value) noexcept
  {
    double offset = m_cc_lo;
    if(m_cc_hi > m_cc_lo)
      offset += double(uniform()) * (m_cc_hi - m_cc_lo);
    return std::clamp(int(std::lround(double(value) + offset)), 0, 127);
  }

  void note_off(const libremidi::message& m, int64_t arrival) noexcept
  {
    const int pitch = m.bytes[1];
    const int channel = m.get_channel();

    const auto it = std::find_if(
        m_active.begin(), m_active.end(), [&](const active_note& n) {
      return n.pitch == pitch && n.channel == channel;
    });

    if(it == m_active.end())
    {
      // A note-off we never saw the note-on for: pass it through so that
      // nothing hangs downstream.
      schedule(arrival + m_latency, m);
      return;
    }

    const active_note note = *it;
    m_active.erase(it);

    if(note.dropped)
      return; // its note-on was never sent

    // Scale the duration rather than the end date, so the note keeps its shape,
    // and make sure it can never end before -- or at the same sample as -- it
    // started.
    const int64_t duration = std::max<int64_t>(1, arrival - note.arrival_on);
    const int64_t scaled
        = std::max<int64_t>(1, int64_t(std::llround(double(duration) * note.length_scale)));

    auto out = m;
    out.bytes[1] = note.out_pitch;
    schedule(note.on_at + scaled, out, note.id);
  }


  // --------------------------------------------------------------- settings

  void cache_parameters() noexcept
  {
    const int seed = inputs.seed;
    if(seed != m_seed)
    {
      // Only the random state: the queue holds note-offs for notes that are
      // already sounding, and dropping them here would hang them.
      m_seed = seed;
      reseed();
    }

    m_timing_samples = int64_t(std::llround(double(inputs.timing) * m_rate));
    m_spread_samples = int64_t(std::llround(double(inputs.spread) * m_rate));
    m_chord_window = int64_t(std::llround(double(inputs.chord_window) * m_rate));

    // Three standard deviations of headroom is enough for the deviation to be
    // essentially symmetric; below that, early notes get clamped to their
    // original date.
    m_latency = int64_t(
        std::llround(3.0 * double(inputs.early) * double(m_timing_samples)));

    const float colour = std::clamp((float)inputs.colour, 0.f, 2.f);
    m_timing_noise.set_beta(colour);
    m_velocity_noise.set_beta(colour);
    m_length_noise.set_beta(colour);
    m_pitch_noise.set_beta(colour);

    m_velocity = inputs.velocity;
    m_length = std::clamp((float)inputs.length, 0.f, 1.f);
    m_chance = std::clamp((float)inputs.chance, 0.f, 1.f);
    m_pitch = std::clamp(int(inputs.pitch), 0, 12);
    m_channel = inputs.channel;

    const auto [vlo, vhi] = inputs.velocity_range.value;
    m_vel_lo = std::clamp(int(std::lround(vlo)), 1, 127);
    m_vel_hi = std::clamp(int(std::lround(vhi)), m_vel_lo, 127);

    const auto [klo, khi] = inputs.key_range.value;
    m_key_lo = std::clamp(int(std::lround(klo)), 0, 127);
    m_key_hi = std::clamp(int(std::lround(khi)), m_key_lo, 127);

    m_chance_unit = inputs.chance_unit.value;
    m_chance_period_s = std::max(0.001, double(inputs.chance_period.value));
    m_velocity_chance = inputs.velocity_chance.value;

    m_euclid = inputs.euclid.value;
    m_euclid_steps = std::clamp(int(inputs.euclid_steps.value), 1, 64);
    m_euclid_pulses = std::clamp(int(inputs.euclid_pulses.value), 0, 64);
    m_euclid_rotation = std::clamp(int(inputs.euclid_rotation.value), 0, 63);
    m_euclid_step_s = std::max(0.001, double(inputs.euclid_step.value));

    m_random_cc = std::clamp(int(inputs.random_cc.value), -1, 127);
    const auto [clo, chi] = inputs.random_cc_range.value;
    m_cc_lo = std::clamp(double(clo), -127., 127.);
    m_cc_hi = std::clamp(double(chi), m_cc_lo, 127.);
  }

  //! Random state only. Safe to call while notes are in flight: it touches
  //! nothing that has already been scheduled.
  void reseed() noexcept
  {
    m_timing_noise.reset();
    m_velocity_noise.reset();
    m_length_noise.reset();
    m_pitch_noise.reset();
    m_have_spare = false;
    m_chord_open = false;
    m_chord_index = 0;
    m_chord_offset = 0;
    m_chord_draw = 0.f;
    m_period_valid = false;

    if(m_seed != 0)
      m_rng.seed(uint64_t(m_seed), UINT64_C(0x9e3779b97f4a7c15));
    else
      m_rng.seed(m_free_seed[0], m_free_seed[1]);
  }

  //! Drops everything, including anything queued. Only safe when nothing can be
  //! left sounding downstream -- call flush_sounding() first otherwise.
  void full_reset() noexcept
  {
    m_pending.clear();
    m_active.clear();
    m_now = 0;
    m_last_position = 0;
    reseed();
  }

  static bool is_note_off_msg(const libremidi::message& m) noexcept
  {
    const auto t = m.get_message_type();
    if(t == libremidi::message_type::NOTE_OFF)
      return true;
    return t == libremidi::message_type::NOTE_ON && m.size() >= 3 && m.bytes[2] == 0;
  }

  //! Send a note-off for everything that may still be sounding downstream:
  //! the note-offs we have queued but not yet released, plus the notes whose
  //! note-off has not even arrived. Dropping the queue without this hangs them.
  void flush_sounding(int ts) noexcept
  {
    // Queued controller state goes out -- a sustain-pedal release, say --
    // since dropping it would leave that state stuck downstream. All-notes-off
    // does not reset the pedal.
    for(const auto& e : m_pending)
    {
      const auto type = e.msg.get_message_type();
      const bool is_note = type == libremidi::message_type::NOTE_ON
                           || type == libremidi::message_type::NOTE_OFF;
      if(!is_note)
      {
        auto msg = e.msg;
        msg.timestamp = ts;
        outputs.midi.push_back(std::move(msg));
      }
    }

    // Then every note the output stage knows to be sounding is ended: exactly
    // those, no stray note-off for notes still queued or never sent.
    for(int c = 0; c < 16; c++)
      for(int p = 0; p < 128; p++)
        if(m_sounding[c][p] != 0)
        {
          auto msg = libremidi::channel_events::note_off(c + 1, p, 0);
          msg.timestamp = ts;
          outputs.midi.push_back(std::move(msg));
          m_sounding[c][p] = 0;
        }
  }


  // ------------------------------------------------------------------ state

  ossia::small_vector<scheduled, max_pending> m_pending;
  ossia::small_vector<active_note, max_active> m_active;

  fractional_noise m_timing_noise;
  fractional_noise m_velocity_noise;
  fractional_noise m_length_noise;
  fractional_noise m_pitch_noise;

  //! Per channel and pitch, the note sounding at the output (0: none).
  uint32_t m_sounding[16][128]{};
  uint32_t m_next_id{1};

  rnd::pcg m_rng;
  //! Drawn once at construction, so a free-running seed never needs entropy
  //! from the processing thread.
  uint64_t m_free_seed[2]{};
  float m_spare{};
  bool m_have_spare{};

  double m_rate{48000.};
  int64_t m_now{};
  int64_t m_last_position{};

  int64_t m_chord_anchor{};
  int64_t m_chord_offset{};
  int m_chord_index{};
  bool m_chord_open{};

  // Cached once per tick
  int64_t m_timing_samples{};
  int64_t m_spread_samples{};
  int64_t m_chord_window{};
  int64_t m_latency{};
  int m_velocity{};
  float m_length{};
  float m_chance{1.f};
  int m_pitch{};
  int m_channel{};
  int m_vel_lo{1}, m_vel_hi{127};
  int m_key_lo{0}, m_key_hi{127};
  int m_seed{};

  ChanceUnit m_chance_unit{Note};
  double m_chance_period_s{0.5};
  bool m_velocity_chance{};
  bool m_euclid{};
  int m_euclid_steps{16};
  int m_euclid_pulses{4};
  int m_euclid_rotation{};
  double m_euclid_step_s{0.125};
  int m_random_cc{-1};
  double m_cc_lo{}, m_cc_hi{};

  //! The musical position of the current tick
  double m_tempo{120.};
  double m_quarter_start{};
  double m_quarters_per_frame{};

  float m_chord_draw{};
  int64_t m_period_index{};
  bool m_period_valid{};
  bool m_period_open{true};

  bool m_stop_requested{};
};
}
