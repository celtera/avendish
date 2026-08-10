#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <ossia/detail/small_vector.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/curve.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/sample_accurate_controls.hpp>
#include <libremidi/message.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace mtk
{
/**
 * @brief Turns MIDI notes into control-rate envelopes.
 *
 * The envelope is generated from note-on / note-off events and can then be
 * patched into any control of the software, the same way an LFO would be.
 *
 * Two families of shapes are available:
 *
 * - The AD / AHD / ADSR / AHDSR / DAHDSR stage machine, whose rising and
 *   falling segments are shaped by the "Attack curve" / "Decay curve" knobs.
 *   In those modes the drawn curve acts as a *transfer function* applied to
 *   the envelope output: by default it is the identity, but drawing a
 *   non-monotonic curve folds the envelope back onto itself.
 *
 * - The Curve / CurveSustain modes, where the drawn curve *is* the envelope,
 *   traversed over "Curve duration". CurveSustain splits the curve at
 *   "Sustain point": the curve plays up to that point, holds there while the
 *   note is held, then plays the remainder over the release time.
 *
 * Voices can be monophonic (with the usual note priorities) or polyphonic,
 * in which case each voice is exposed individually on the "Voices" output and
 * summarized on "Out" according to the "Combine" setting.
 */
struct MidiEnvelope
{
public:
  halp_meta(name, "Midi Envelope")
  halp_meta(c_name, "midi_envelope")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(description, "Turn MIDI notes into control-rate envelopes")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-envelope.html")
  halp_meta(uuid, "e58cf441-0106-4862-8abf-c3cee072f861")

  //! Maximum number of simultaneous voices
  static constexpr int max_voices = 32;
  //! Upper bound on the number of values written per processing block.
  //! Keeps the sample-accurate output map within its inline storage.
  static constexpr int max_points_per_tick = 16;

  // Note: the first enumerator of each of these is the default value.
  enum Envelope
  {
    ADSR,
    AD,
    AHD,
    AHDSR,
    DAHDSR,
    Curve,
    CurveSustain
  };
  enum Trigger
  {
    Retrigger,
    Reset,
    Legato
  };
  enum Voicing
  {
    Mono,
    Poly
  };
  enum Priority
  {
    Last,
    First,
    Lowest,
    Highest
  };
  enum Combine
  {
    Max,
    Sum,
    Average,
    Latest
  };

  struct ins
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;

    struct : halp::enum_t<Envelope, "Envelope">
    {
      enum widget
      {
        combobox
      };
      halp_meta(
          description, "Envelope stages. Curve modes use the drawn curve as the shape.")
    } envelope;

    struct : halp::curve_port<"Curve">
    {
      halp_meta(
          description,
          "Envelope shape in Curve modes, output transfer function otherwise")
    } curve;

    struct : halp::time_chooser<"Delay", halp::range{0., 10., 0.}>
    {
      halp_meta(description, "Time before the attack starts (DAHDSR only)")
    } delay;

    halp::time_chooser<"Attack", halp::range{0., 20., 0.01}> attack;

    struct : halp::time_chooser<"Hold", halp::range{0., 20., 0.}>
    {
      halp_meta(description, "Time spent at the peak before decaying (AHD, AHDSR, DAHDSR)")
    } hold;

    halp::time_chooser<"Decay", halp::range{0., 20., 0.2}> decay;

    struct : halp::hslider_f32<"Sustain", halp::range{0., 1., 0.7}>
    {
      halp_meta(description, "Level held while the note is down, relative to the peak")
    } sustain;

    halp::time_chooser<"Release", halp::range{0., 20., 0.3}> release;

    struct : halp::time_chooser<"Curve duration", halp::range{0.001, 60., 1.}>
    {
      halp_meta(description, "Time taken to traverse the drawn curve (Curve modes)")
    } curve_duration;

    struct : halp::hslider_f32<"Sustain point", halp::range{0., 1., 0.5}>
    {
      halp_meta(description, "Position in the drawn curve held while the note is down")
    } sustain_point;

    struct : halp::knob_f32<"Attack curve", halp::range{-1., 1., 0.}>
    {
      halp_meta(description, "Negative: fast then slow. Positive: slow then fast.")
    } attack_curve;

    struct : halp::knob_f32<"Decay curve", halp::range{-1., 1., -0.5}>
    {
      halp_meta(description, "Shape of the decay and release segments")
    } decay_curve;

    struct : halp::enum_t<Trigger, "Trigger">
    {
      enum widget
      {
        combobox
      };
      halp_meta(
          description, "Retrigger: restart from the current level. Reset: restart from "
                       "zero. Legato: do not restart if a note is already held.")
    } trigger;

    struct : halp::enum_t<Voicing, "Voicing">
    {
      enum widget
      {
        combobox
      };
    } voicing;

    struct : halp::spinbox_i32<"Voices", halp::range{1, max_voices, 8}>
    {
      halp_meta(description, "Number of voices in polyphonic mode")
    } voice_count;

    struct : halp::enum_t<Priority, "Note priority">
    {
      enum widget
      {
        combobox
      };
      halp_meta(description, "Which of the held notes drives the envelope in mono mode")
    } priority;

    struct : halp::enum_t<Combine, "Combine">
    {
      enum widget
      {
        combobox
      };
      halp_meta(description, "How the voices are summarized on the Out port")
    } combine;

    struct : halp::spinbox_i32<"Channel", halp::range{0, 16, 0}>
    {
      halp_meta(description, "MIDI channel (0 = all)")
    } channel;

    struct : halp::range_slider_f32<
                 "Key range", halp::range_slider_range{0., 127., {0., 127.}}>
    {
      halp_meta(description, "Only notes within this range trigger the envelope")
    } key_range;

    struct : halp::knob_f32<"Velocity amount", halp::range{0., 1., 1.}>
    {
      halp_meta(description, "How much note velocity scales the envelope peak")
    } velocity_amount;

    struct : halp::knob_f32<"Key tracking", halp::range{-1., 1., 0.}>
    {
      halp_meta(
          description,
          "Positive: higher notes give shorter envelopes, one octave per halving")
    } key_tracking;

    struct : halp::range_slider_f32<
                 "Output range", halp::range_slider_range{0., 1., {0., 1.}}>
    {
      halp_meta(description, "The envelope is mapped from 0-1 into this range")
    } output_range;

    struct : halp::time_chooser<"Smooth", halp::range{0., 1., 0.}>
    {
      halp_meta(description, "Slew applied to the output")
    } smooth;

    struct : halp::time_chooser<"Resolution", halp::range{0.0005, 0.1, 0.002}>
    {
      halp_meta(description, "Interval between values emitted inside a processing block")
    } resolution;
  } inputs;

  struct
  {
    halp::accurate<halp::val_port_01<"Out", float>> out;
    halp::val_port_01<"Gate", float> gate;
    // Raw MIDI note number, not a 0-1 port. The declared range becomes the
    // ossia port domain at execution time, so it has to be the real one.
    struct : halp::val_port<"Pitch", std::optional<float>>
    {
      struct range
      {
        const float min = 0.f;
        const float max = 127.f;
        const float init = 0.f;
      };
    } pitch;

    halp::val_port_01<"Velocity", std::optional<float>> velocity;

    struct : halp::val_port<"Active", int>
    {
      struct range
      {
        const int min = 0;
        const int max = max_voices;
        const int init = 0;
      };
    } active;

    halp::val_port<"Voices", std::vector<float>> voices;
    halp::midi_bus<"MIDI out", libremidi::message> midi;
  } outputs;

  //! The per-voice output is a std::vector, and emit_auxiliary_outputs resizes
  //! it whenever the voice count changes. Reserving the maximum here keeps that
  //! resize from ever reallocating on the audio thread.
  MidiEnvelope() { outputs.voices.value.reserve(max_voices); }

  void prepare(halp::setup info) noexcept
  {
    if(info.rate > 0.)
      m_rate = info.rate;
    reset();
  }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical t) noexcept
  {
    const int frames = t.frames > 0 ? t.frames : 0;

    cache_parameters();

    if(frames <= 0)
      return;

    // Emit at most max_points_per_tick values per block: this keeps the
    // sample-accurate output within its inline storage, and degrades to a
    // single value per block when the resolution is coarser than the block.
    const int interval
        = std::max({1, m_resolution_samples, (frames + max_points_per_tick - 1)
                                                 / max_points_per_tick});
    // Emissions are `interval` apart inside a block, but never further apart
    // than the block itself: when the resolution is coarser than the buffer we
    // still emit once per buffer, so that is the real time step.
    m_smooth_alpha = alpha_for(std::min(interval, frames));

    const auto& msgs = inputs.midi.midi_messages;
    const int n_msgs = (int)msgs.size();

    int cursor = 0;
    int mi = 0;
    while(cursor < frames)
    {
      // Consume every message due at or before the cursor
      while(mi < n_msgs && message_time(msgs[mi], frames) <= cursor)
      {
        handle_message(msgs[mi]);
        ++mi;
      }

      // Next stopping point: either the next message or the next emission
      const int next_msg
          = (mi < n_msgs) ? message_time(msgs[mi], frames) : frames;
      const int next_emit = std::min(frames, cursor + (interval - cursor % interval));
      const int target = std::min({next_msg, next_emit, frames});

      // Guaranteed to make progress: both candidates are strictly > cursor
      advance_all(target - cursor);
      cursor = target;

      if(cursor == next_emit)
        emit(std::min(cursor, frames - 1));
    }

    // Messages landing exactly at the end of the block
    while(mi < n_msgs)
    {
      handle_message(msgs[mi]);
      ++mi;
    }

    emit_auxiliary_outputs();
  }

  struct ui
  {
    halp_meta(name, "Midi Envelope")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    // The stages, laid out in the order they are traversed.
    struct
    {
      halp_meta(name, "Stages")
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)

      halp::item<&ins::envelope> envelope;

      struct
      {
        halp_meta(layout, halp::layouts::grid)
        halp_meta(columns, 3)

        halp::item<&ins::delay> delay;
        halp::item<&ins::attack> attack;
        halp::item<&ins::hold> hold;
        halp::item<&ins::decay> decay;
        halp::item<&ins::sustain> sustain;
        halp::item<&ins::release> release;
      } stages;

      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::item<&ins::attack_curve> attack_curve;
        halp::item<&ins::decay_curve> decay_curve;
      } shaping;
    } stages;

    // The drawn curve, with the two controls that only apply to it.
    struct
    {
      halp_meta(name, "Curve")
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)

      halp::item<&ins::curve> curve;

      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::item<&ins::curve_duration> duration;
        halp::item<&ins::sustain_point> sustain_point;
      } curve_params;
    } curve;

    // Everything else, kept out of the way in tabs.
    struct
    {
      halp_meta(name, "Settings")
      halp_meta(layout, halp::layouts::tabs)
      halp_meta(background, halp::colors::background_darker)

      struct
      {
        halp_meta(name, "Voices")
        halp_meta(layout, halp::layouts::vbox)

        halp::item<&ins::voicing> voicing;
        halp::item<&ins::voice_count> voice_count;
        halp::item<&ins::trigger> trigger;
        halp::item<&ins::priority> priority;
        halp::item<&ins::combine> combine;
      } voices;

      struct
      {
        halp_meta(name, "Notes")
        halp_meta(layout, halp::layouts::vbox)

        halp::item<&ins::channel> channel;
        halp::item<&ins::key_range> key_range;

        struct
        {
          halp_meta(layout, halp::layouts::hbox)
          halp::item<&ins::velocity_amount> velocity_amount;
          halp::item<&ins::key_tracking> key_tracking;
        } modulation;
      } notes;

      struct
      {
        halp_meta(name, "Output")
        halp_meta(layout, halp::layouts::vbox)

        halp::item<&ins::output_range> output_range;
        halp::item<&ins::smooth> smooth;
        halp::item<&ins::resolution> resolution;
      } output;
    } settings;
  };

private:
  enum class stage
  {
    idle,
    delay,
    attack,
    hold,
    decay,
    sustain,
    release,
    curve_rise,
    curve_hold,
    curve_fall
  };

  struct voice
  {
    int note{-1};
    float velocity{};   //!< 0-1
    float peak{1.f};    //!< velocity-scaled maximum level
    float from{};       //!< level when the current stage was entered
    float level{};      //!< current level, 0-1
    double t{};         //!< samples elapsed in the current stage
    double time_scale{1.}; //!< key tracking multiplier
    int64_t age{};      //!< allocation order, for note stealing
    bool held{};
    stage st{stage::idle};
  };

  struct held_note
  {
    int note{};
    float velocity{};
  };

  // ---------------------------------------------------------------- helpers

  bool is_curve_mode() const noexcept
  {
    return m_envelope == Curve || m_envelope == CurveSustain;
  }
  bool has_sustain_stage() const noexcept
  {
    switch(m_envelope)
    {
      case ADSR:
      case AHDSR:
      case DAHDSR:
      case CurveSustain:
        return true;
      default:
        return false;
    }
  }
  bool has_hold_stage() const noexcept
  {
    return m_envelope == AHD || m_envelope == AHDSR || m_envelope == DAHDSR;
  }

  //! Shaping of the normalized progress within a segment.
  //! curve == 0 is linear, > 0 is convex (slow then fast), < 0 concave.
  static float shape(float u, float curve) noexcept
  {
    u = std::clamp(u, 0.f, 1.f);
    if(curve > -1e-4f && curve < 1e-4f)
      return u;
    return std::pow(u, std::pow(8.f, curve));
  }

  //! The drawn curve, defaulting to the identity when it is empty
  float curve_at(float x) noexcept
  {
    auto& c = inputs.curve.value;
    x = std::clamp(x, 0.f, 1.f);
    if(c.empty())
      return x;
    return c.value_at(x);
  }

  static int message_time(const libremidi::message& m, int frames) noexcept
  {
    return (int)std::clamp<int64_t>(m.timestamp, 0, frames);
  }

  //! One-pole coefficient for a slew of m_smooth seconds over n samples
  float alpha_for(int n) const noexcept
  {
    if(m_smooth <= 0.)
      return 1.f;
    return (float)(1. - std::exp(-double(n) / (m_smooth * m_rate)));
  }

  void cache_parameters() noexcept
  {
    m_envelope = inputs.envelope.value;
    m_trigger = inputs.trigger.value;
    m_voicing = inputs.voicing.value;
    m_priority = inputs.priority.value;
    m_combine = inputs.combine.value;

    // score does not clamp control inlets to their declared domain, so a preset
    // can supply anything at all. Everything used as a duration, an exponent or
    // a loop bound is clamped here rather than trusted.
    const auto seconds = [](auto v) {
      return std::isfinite((float)v) ? std::clamp((float)v, 0.f, 3600.f) : 0.f;
    };

    m_delay = seconds(inputs.delay);
    m_attack = seconds(inputs.attack);
    m_hold = seconds(inputs.hold);
    m_decay = seconds(inputs.decay);
    m_sustain = std::clamp((float)inputs.sustain, 0.f, 1.f);
    m_release = seconds(inputs.release);
    m_duration = std::max(1e-4f, seconds(inputs.curve_duration));
    m_sustain_point = std::clamp((float)inputs.sustain_point, 0.f, 1.f);

    // Feeds pow(8, curve): an out-of-range value would give an infinite exponent
    m_attack_curve = std::clamp((float)inputs.attack_curve, -1.f, 1.f);
    m_decay_curve = std::clamp((float)inputs.decay_curve, -1.f, 1.f);
    m_vel_amount = std::clamp((float)inputs.velocity_amount, 0.f, 1.f);
    // Feeds pow(2, -keytrack * semitones): likewise, and an infinite time_scale
    // freezes the voice in whatever stage it is in.
    m_keytrack = std::clamp((float)inputs.key_tracking, -1.f, 1.f);

    const auto [lo, hi] = inputs.output_range.value;
    m_out_lo = std::isfinite(lo) ? lo : 0.f;
    m_out_hi = std::isfinite(hi) ? hi : 1.f;

    m_smooth = seconds(inputs.smooth);
    m_resolution_samples
        = (int)std::lround(std::clamp(double(seconds(inputs.resolution)) * m_rate, 1., 1e7));

    const int requested
        = std::clamp((int)inputs.voice_count, 1, max_voices);
    const int wanted = (m_voicing == Mono) ? 1 : requested;

    if(wanted < m_voice_count)
    {
      // Every loop below runs over [0; m_voice_count[. Voices that drop out of
      // that range would otherwise never be advanced or released again, and
      // would come back at their frozen level as soon as the count grows.
      for(int i = wanted; i < m_voice_count; i++)
        m_voices[i] = {};
    }
    m_voice_count = wanted;
  }

  // -------------------------------------------------------- envelope stages

  stage first_stage() const noexcept
  {
    if(is_curve_mode())
      return stage::curve_rise;
    if(m_envelope == DAHDSR)
      return stage::delay;
    return stage::attack;
  }

  //! A holding stage waits for a note-off, so it may only be entered while the
  //! note is actually down. Otherwise a voice whose note-off was ignored -- a
  //! percussive mode, or a one-shot curve -- would land in one when the mode is
  //! switched mid-flight and stay there for good: nothing would ever release it.
  stage next_stage(const voice& v, stage s) const noexcept
  {
    switch(s)
    {
      case stage::delay:
        return stage::attack;
      case stage::attack:
        return has_hold_stage() ? stage::hold : stage::decay;
      case stage::hold:
        return stage::decay;
      case stage::decay:
        if(!has_sustain_stage())
          return stage::idle;
        return v.held ? stage::sustain : stage::release;
      case stage::release:
        return stage::idle;
      case stage::curve_rise:
        if(m_envelope != CurveSustain)
          return stage::idle;
        return v.held ? stage::curve_hold : stage::curve_fall;
      case stage::curve_fall:
        return stage::idle;
      default:
        return stage::idle;
    }
  }

  //! Stages that wait for a note-off instead of elapsing
  static bool is_holding_stage(stage s) noexcept
  {
    return s == stage::sustain || s == stage::curve_hold;
  }

  //! Duration of a stage, in samples
  double stage_duration(const voice& v, stage s) const noexcept
  {
    const double sc = v.time_scale * m_rate;
    switch(s)
    {
      case stage::delay:
        return m_delay * sc;
      case stage::attack:
        return m_attack * sc;
      case stage::hold:
        return m_hold * sc;
      case stage::decay:
        return m_decay * sc;
      case stage::release:
        return m_release * sc;
      case stage::curve_rise:
        // In one-shot mode the whole curve is traversed, otherwise only the
        // part before the sustain point.
        return m_duration * sc
               * ((m_envelope == CurveSustain) ? m_sustain_point : 1.f);
      case stage::curve_fall:
        return m_release * sc;
      default:
        return 0.;
    }
  }

  //! Target level reached at the end of a stage
  float stage_target(const voice& v, stage s) const noexcept
  {
    switch(s)
    {
      case stage::delay:
        return v.from; // flat
      case stage::attack:
        return v.peak;
      case stage::hold:
        return v.from; // flat, entered at the peak
      case stage::decay:
        return v.peak * m_sustain;
      case stage::sustain:
        return v.peak * m_sustain;
      case stage::release:
        return 0.f;
      default:
        return v.from;
    }
  }

  float level_for(voice& v) noexcept
  {
    switch(v.st)
    {
      case stage::idle:
        return 0.f;

      case stage::curve_rise:
      case stage::curve_hold:
      case stage::curve_fall: {
        const float sp = (m_envelope == CurveSustain) ? m_sustain_point : 1.f;
        float pos = sp;
        if(v.st == stage::curve_rise)
        {
          const double dur = stage_duration(v, v.st);
          const float u = (dur <= 0.) ? 1.f : (float)std::clamp(v.t / dur, 0., 1.);
          pos = u * sp;
        }
        else if(v.st == stage::curve_fall)
        {
          const double dur = stage_duration(v, v.st);
          const float u = (dur <= 0.) ? 1.f : (float)std::clamp(v.t / dur, 0., 1.);
          pos = sp + u * (1.f - sp);
        }
        return curve_at(pos) * v.peak;
      }

      case stage::sustain:
        return v.peak * m_sustain;

      default: {
        const double dur = stage_duration(v, v.st);
        const float u = (dur <= 0.) ? 1.f : (float)std::clamp(v.t / dur, 0., 1.);
        const float c
            = (v.st == stage::attack) ? m_attack_curve : m_decay_curve;
        const float target = stage_target(v, v.st);
        return v.from + (target - v.from) * shape(u, c);
      }
    }
  }

  void advance(voice& v, double n) noexcept
  {
    if(v.st == stage::idle)
      return;

    // Bounded: every iteration either consumes time or moves to a later
    // stage, and the stage chain always terminates on idle.
    for(int guard = 0; n > 0. && guard < 64; ++guard)
    {
      if(is_holding_stage(v.st))
        break;

      const double dur = stage_duration(v, v.st);
      const double remaining = dur - v.t;
      if(n < remaining)
      {
        v.t += n;
        break;
      }

      n -= std::max(0., remaining);
      v.t = dur;
      v.from = level_for(v); // level reached at the end of the stage
      v.st = next_stage(v, v.st);
      v.t = 0.;

      if(v.st == stage::idle)
      {
        v.level = 0.f;
        v.note = -1;
        v.held = false;
        return;
      }
    }

    v.level = level_for(v);
  }

  void advance_all(int n) noexcept
  {
    if(n <= 0)
      return;
    for(int i = 0; i < m_voice_count; i++)
      advance(m_voices[i], n);
  }

  // ------------------------------------------------------------- triggering

  //! @param legato true when another note was already down. Legato suppresses
  //! the restart only then -- a still-ringing release tail is not a held note,
  //! so a detached note must retrigger even under a long release.
  void trigger(voice& v, int note, float vel, bool legato) noexcept
  {
    v.note = note;
    v.held = true;
    v.velocity = vel;
    v.peak = 1.f - m_vel_amount + m_vel_amount * vel;
    v.time_scale
        = (m_keytrack != 0.f)
              ? std::pow(2., -double(m_keytrack) * double(note - 60) / 12.)
              : 1.;
    v.age = ++m_age_counter;

    if(m_trigger == Legato && legato && v.st != stage::idle)
      return; // retarget only: the envelope keeps running

    v.from = (m_trigger == Reset) ? 0.f : v.level;
    if(m_trigger == Reset)
      v.level = 0.f;
    v.t = 0.;
    v.st = first_stage();
    v.level = level_for(v);
  }

  void release_voice(voice& v) noexcept
  {
    v.held = false;

    switch(v.st)
    {
      case stage::idle:
        return;

      // One-shot shapes ignore the note-off and play to completion
      case stage::curve_rise:
        if(m_envelope != CurveSustain)
          return;
        [[fallthrough]];
      case stage::curve_hold:
        v.from = v.level;
        v.t = 0.;
        v.st = stage::curve_fall;
        return;

      case stage::curve_fall:
        return;

      default:
        // Keyed on the voice, not on the current mode. Switching the envelope
        // type to a one-shot one while a note is held would otherwise strand a
        // voice in sustain -- a holding stage that advance() never leaves, so
        // the voice would stay at its sustain level for good.
        if(v.st != stage::sustain && !has_sustain_stage())
          return; // AD / AHD: percussive, let it finish
        v.from = v.level;
        v.t = 0.;
        v.st = stage::release;
        return;
    }
  }

  void handle_message(const libremidi::message& m) noexcept
  {
    outputs.midi.push_back(m);

    const int channel = inputs.channel;
    if(channel != 0 && m.get_channel() != channel)
      return;

    const auto type = m.get_message_type();
    switch(type)
    {
      case libremidi::message_type::NOTE_ON: {
        if(m.size() < 3)
          return;
        const int note = m.bytes[1];
        const int vel = m.bytes[2];
        if(vel == 0)
          note_off(note);
        else
          note_on(note, vel / 127.f);
        return;
      }
      case libremidi::message_type::NOTE_OFF: {
        if(m.size() < 2)
          return;
        note_off(m.bytes[1]);
        return;
      }
      case libremidi::message_type::CONTROL_CHANGE: {
        // All sound off / all notes off
        if(m.size() >= 2 && (m.bytes[1] == 120 || m.bytes[1] == 123))
          all_notes_off();
        return;
      }
      default:
        return;
    }
  }

  void note_on(int note, float vel) noexcept
  {
    const auto [lo, hi] = inputs.key_range.value;
    if(note < (int)std::lround(lo) || note > (int)std::lround(hi))
      return;

    // The held-note stack tracks what is physically down: it drives the gate
    // output in both modes, and the note priority in monophonic mode.
    erase_held(note);
    // Bounded, but the note just pressed must always make it in -- otherwise it
    // would lose the priority contest against a note it should have replaced and
    // simply never sound. Drop the oldest key instead; poly release does not go
    // through this table, so nothing is stranded by that.
    if((int)m_held.size() >= max_voices)
      m_held.erase(m_held.begin());
    m_held.push_back({note, vel});

    if(m_voicing == Mono)
      retarget_mono(/* pressed */ note);
    else
      // Legato is a monophonic notion: in poly every note gets its own voice.
      trigger(m_voices[allocate(note)], note, vel, /* legato */ false);
  }

  void note_off(int note) noexcept
  {
    const bool was_held = erase_held(note);

    if(m_voicing == Mono)
    {
      if(!was_held)
        return; // not a note we were tracking

      if(m_held.empty())
        release_voice(m_voices[0]);
      else
        // Fall back to another held note without restarting the envelope
        retarget_mono(/* pressed */ -1);
    }
    else
    {
      // Deliberately not gated on was_held: the held stack is bounded, and a
      // note that overflowed it must still be able to release its voice.
      for(int i = 0; i < m_voice_count; i++)
        if(m_voices[i].note == note && m_voices[i].held)
          release_voice(m_voices[i]);
    }
  }

  //! Remove a note from the held stack. Returns true if it was there.
  bool erase_held(int note) noexcept
  {
    const auto it = std::remove_if(
        m_held.begin(), m_held.end(),
        [=](const held_note& h) { return h.note == note; });
    if(it == m_held.end())
      return false;
    m_held.erase(it, m_held.end());
    return true;
  }

  void all_notes_off() noexcept
  {
    m_held.clear();
    for(int i = 0; i < m_voice_count; i++)
      release_voice(m_voices[i]);
  }

  //! Pick the held note that should drive the monophonic envelope
  const held_note* selected_note() const noexcept
  {
    if(m_held.empty())
      return nullptr;

    switch(m_priority)
    {
      case First:
        return &m_held.front();
      case Lowest:
        return &*std::min_element(
            m_held.begin(), m_held.end(),
            [](const held_note& a, const held_note& b) { return a.note < b.note; });
      case Highest:
        return &*std::max_element(
            m_held.begin(), m_held.end(),
            [](const held_note& a, const held_note& b) { return a.note < b.note; });
      case Last:
      default:
        return &m_held.back();
    }
  }

  //! @param pressed the note that was just played, or -1 on a note-off.
  //! The envelope only restarts when the note that was just pressed is the one
  //! that wins the priority contest: under Highest priority, pressing a note
  //! below the one already sounding must not retrigger anything.
  void retarget_mono(int pressed) noexcept
  {
    const held_note* sel = selected_note();
    if(!sel)
      return;

    voice& v = m_voices[0];
    if(pressed >= 0 && sel->note == pressed)
    {
      // More than the note just pressed is down, i.e. this is a legato move
      trigger(v, sel->note, sel->velocity, /* legato */ m_held.size() > 1);
    }
    else
    {
      // Only update what the running envelope depends on
      v.note = sel->note;
      v.held = true;
      v.velocity = sel->velocity;
      v.peak = 1.f - m_vel_amount + m_vel_amount * sel->velocity;
      v.time_scale
          = (m_keytrack != 0.f)
                ? std::pow(2., -double(m_keytrack) * double(sel->note - 60) / 12.)
                : 1.;
    }
  }

  //! Find a voice for a new note: same note, then free, then oldest
  int allocate(int note) noexcept
  {
    for(int i = 0; i < m_voice_count; i++)
      if(m_voices[i].note == note && m_voices[i].held)
        return i;

    for(int i = 0; i < m_voice_count; i++)
      if(m_voices[i].st == stage::idle)
        return i;

    int oldest = 0;
    for(int i = 1; i < m_voice_count; i++)
      if(m_voices[i].age < m_voices[oldest].age)
        oldest = i;
    return oldest;
  }

  // ---------------------------------------------------------------- outputs

  float combine_voices() const noexcept
  {
    if(m_voicing == Mono)
      return m_voices[0].level;

    float acc = 0.f;
    int n = 0;
    int latest = -1;
    int64_t latest_age = -1;
    for(int i = 0; i < m_voice_count; i++)
    {
      const voice& v = m_voices[i];
      if(v.st == stage::idle)
        continue;
      ++n;
      switch(m_combine)
      {
        case Max:
          acc = std::max(acc, v.level);
          break;
        case Sum:
        case Average:
          acc += v.level;
          break;
        case Latest:
          if(v.age > latest_age)
          {
            latest_age = v.age;
            latest = i;
          }
          break;
      }
    }

    switch(m_combine)
    {
      case Average:
        return (n > 0) ? acc / n : 0.f;
      case Latest:
        return (latest >= 0) ? m_voices[latest].level : 0.f;
      case Sum:
        return std::min(acc, 1.f);
      case Max:
      default:
        return acc;
    }
  }

  void emit(int ts) noexcept
  {
    float v = std::clamp(combine_voices(), 0.f, 1.f);

    // In the stage-based modes the drawn curve acts as a transfer function
    if(!is_curve_mode())
      v = curve_at(v);

    m_smoothed += (v - m_smoothed) * m_smooth_alpha;

    outputs.out.values[ts] = m_out_lo + (m_out_hi - m_out_lo) * m_smoothed;
  }

  void emit_auxiliary_outputs() noexcept
  {
    int active = 0;
    int newest = -1;
    int64_t newest_age = -1;
    for(int i = 0; i < m_voice_count; i++)
    {
      const voice& v = m_voices[i];
      if(v.st == stage::idle)
        continue;
      ++active;
      if(v.age > newest_age)
      {
        newest_age = v.age;
        newest = i;
      }
    }

    outputs.gate = m_held.empty() ? 0.f : 1.f;
    // .value, not operator=: these ports are derived structs, whose implicit
    // copy-assignment hides the base's operator=.
    outputs.active.value = active;

    if(newest >= 0 && m_voices[newest].note >= 0)
    {
      const float p = m_voices[newest].note;
      const float vel = m_voices[newest].velocity;
      if(p != m_last_pitch)
      {
        outputs.pitch.value = p;
        m_last_pitch = p;
      }
      if(vel != m_last_velocity)
      {
        outputs.velocity = vel;
        m_last_velocity = vel;
      }
    }

    auto& out = outputs.voices.value;
    if((int)out.size() != m_voice_count)
      out.resize(m_voice_count);
    for(int i = 0; i < m_voice_count; i++)
      out[i] = m_out_lo + (m_out_hi - m_out_lo) * std::clamp(m_voices[i].level, 0.f, 1.f);
  }

  void reset() noexcept
  {
    for(auto& v : m_voices)
      v = {};
    m_held.clear();
    m_smoothed = 0.f;
    m_age_counter = 0;
    m_last_pitch = -1.f;
    m_last_velocity = -1.f;
  }

  // ------------------------------------------------------------------ state

  voice m_voices[max_voices]{};
  //! Inline capacity matches the cap enforced in note_on, so holding a lot of
  //! notes never allocates on the audio thread.
  ossia::small_vector<held_note, max_voices> m_held;

  double m_rate{48000.};
  int64_t m_age_counter{};
  float m_smoothed{};
  float m_smooth_alpha{1.f};
  float m_last_pitch{-1.f};
  float m_last_velocity{-1.f};

  // Cached once per tick
  Envelope m_envelope{ADSR};
  Trigger m_trigger{Retrigger};
  Voicing m_voicing{Mono};
  Priority m_priority{Last};
  Combine m_combine{Max};

  float m_delay{}, m_attack{}, m_hold{}, m_decay{}, m_release{};
  float m_sustain{}, m_duration{1.f}, m_sustain_point{0.5f};
  float m_attack_curve{}, m_decay_curve{};
  float m_vel_amount{1.f}, m_keytrack{};
  float m_out_lo{}, m_out_hi{1.f};
  float m_smooth{};
  int m_resolution_samples{96};
  int m_voice_count{1};
};
}
