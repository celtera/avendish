#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>
#include <rnd/random.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

namespace mtk
{
/**
 * @brief Reshapes one kind of MIDI value: velocities, release velocities,
 * controllers, pitch bend, channel or poly pressure.
 *
 * The operations form a chain in a fixed order, each one neutral at its
 * default value:
 *
 *   fixed, rescale, curve, compress, gain (with the key-tracked gain),
 *   offset, random, clip.
 *
 * Values are handled as real numbers on a 0 - 127 scale, so every control
 * means the same thing whatever the kind. Pitch bend keeps its 14 bits: 0 is
 * 0, 64 is the center 8192 and 127 is 16383, and nothing is rounded before the
 * output. Curve, compress and gain act on the distance to the rest value: 0
 * for every kind but pitch bend, whose rest value is its center, so that its
 * two halves are shaped alike.
 *
 * The curve is a single exponent rather than breakpoints: one knob, neutral in
 * the middle, monotonic for any setting, as in the velocity curves of Live and
 * Bitwig. Rescale and clip together give the corners that breakpoints would.
 */
struct MidiValueShaper
{
public:
  halp_meta(name, "Midi value shaper")
  halp_meta(c_name, "midi_value_shaper")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(
      description,
      "Gain, curve, compression, rescaling, key tracking and randomization of "
      "velocities, controllers, pitch bend and pressure")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-value-shaper.html")
  halp_meta(uuid, "6ed46ee7-8d04-4e46-8a68-e2c6633fd217")

  enum Kind
  {
    Velocity,
    Release,
    CC,
    PitchBend,
    Pressure,
    PolyPressure
  };

  struct ins
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;

    // ---- Selection ------------------------------------------------------
    struct : halp::enum_t<Kind, "Kind">
    {
      enum widget
      {
        combobox
      };
      halp_meta(
          description,
          "The value that is shaped: note-on velocity, note-off velocity, a "
          "controller, pitch bend, channel pressure or poly pressure")
    } kind;

    struct : halp::spinbox_i32<"CC or note", halp::range{-1, 127, -1}>
    {
      halp_meta(
          description,
          "Controller number for CC, note for poly pressure (-1: any)")
    } number;

    struct : halp::spinbox_i32<"Channel", halp::range{0, 16, 0}>
    {
      halp_meta(description, "MIDI channel (0: all)")
    } channel;

    // ---- Shape ----------------------------------------------------------
    struct : halp::toggle<"Fixed">
    {
      halp_meta(description, "Replaces every value by Fixed value before the rest")
    } fixed;

    struct : halp::hslider_f32<"Fixed value", halp::range{0., 127., 100.}>
    {
      halp_meta(description, "The value used when Fixed is on")
    } fixed_value;

    struct : halp::range_slider_f32<
                 "In range", halp::range_slider_range{0., 127., {0., 127.}}>
    {
      halp_meta(
          description, "Values are clipped to this range, then mapped to Out range")
    } in_range;

    struct : halp::range_slider_f32<
                 "Out range", halp::range_slider_range{0., 127., {0., 127.}}>
    {
      halp_meta(description, "Where In range is mapped to")
    } out_range;

    struct : halp::knob_f32<"Curve", halp::range{-1., 1., 0.}>
    {
      halp_meta(
          description,
          "Above 0 raises the low values, below 0 lowers them; 0 is linear")
    } curve;

    struct : halp::hslider_f32<"Threshold", halp::range{0., 127., 64.}>
    {
      halp_meta(
          description,
          "Values above it are compressed or expanded by Ratio. For pitch bend it "
          "is a distance to the center, 127 being a full bend.")
    } threshold;

    struct : halp::knob_f32<"Ratio", halp::range{0.25, 8., 1.}>
    {
      halp_meta(
          description, "Above 1 compresses, below 1 expands what is above Threshold")
    } ratio;

    // ---- Gain -----------------------------------------------------------
    struct : halp::hslider_f32<"Gain", halp::range{0., 400., 100.}>
    {
      halp_meta(description, "Amplification, in percent")
    } gain;

    struct : halp::hslider_f32<"Offset", halp::range{-127., 127., 0.}>
    {
      halp_meta(description, "Added to the value after the gain")
    } offset;

    struct : halp::knob_f32<"Low keys", halp::range{0., 200., 100.}>
    {
      halp_meta(
          description,
          "Gain in percent at note 24, for the kinds that come with a note. "
          "Between 24, 60 and 96 the gains are interpolated.")
    } low_gain;

    struct : halp::knob_f32<"Mid keys", halp::range{0., 200., 100.}>
    {
      halp_meta(description, "Gain in percent at note 60")
    } mid_gain;

    struct : halp::knob_f32<"High keys", halp::range{0., 200., 100.}>
    {
      halp_meta(description, "Gain in percent at note 96")
    } high_gain;

    // ---- Limits ---------------------------------------------------------
    struct : halp::range_slider_f32<
                 "Random add", halp::range_slider_range{-64., 64., {0., 0.}}>
    {
      halp_meta(description, "A random amount drawn uniformly in this range is added")
    } random;

    struct : halp::spinbox_i32<"Seed", halp::range{0, 10000, 0}>
    {
      halp_meta(
          description,
          "0 keeps running freely; any other value gives a reproducible sequence, "
          "restarted whenever the transport rewinds or stops")
    } seed;

    struct : halp::range_slider_f32<
                 "Clip", halp::range_slider_range{0., 127., {0., 127.}}>
    {
      halp_meta(
          description, "The result is kept in this range. Note-on velocities never "
                       "go below 1, so a note is never turned into a note-off.")
    } clip;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;
  } outputs;

  //! std::random_device can allocate, block and throw: it is only used here.
  MidiValueShaper()
  {
    std::random_device rd;
    m_free_seed[0] = (uint64_t(rd()) << 32) ^ uint64_t(rd());
    m_free_seed[1] = (uint64_t(rd()) << 32) ^ uint64_t(rd());
    reseed();
  }

  //! The next tick restarts the random sequence of a seeded take.
  void stop() noexcept { m_stop_requested = true; }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical t) noexcept
  {
    const int seed = inputs.seed;
    if(seed != m_seed || m_stop_requested
       || (seed != 0 && t.position_in_frames < m_last_position))
    {
      m_seed = seed;
      m_stop_requested = false;
      reseed();
    }
    m_last_position = t.position_in_frames;

    for(const auto& m : inputs.midi)
    {
      auto out = m;
      process(out);
      outputs.midi.push_back(std::move(out));
    }
  }

  //! Shapes one value, on the 0 - 127 scale. `note` is the note the value
  //! belongs to, or -1.
  double shape(double u, int note) noexcept
  {
    const auto& in = inputs;
    const bool bipolar = in.kind.value == PitchBend;

    if(in.fixed.value)
      u = in.fixed_value.value;

    // Rescale
    {
      const auto [ilo_f, ihi_f] = in.in_range.value;
      const auto [olo_f, ohi_f] = in.out_range.value;
      const double ilo = std::min(ilo_f, ihi_f), ihi = std::max(ilo_f, ihi_f);
      const double olo = olo_f, ohi = ohi_f;
      u = std::clamp(u, ilo, ihi);
      if(ilo != 0. || ihi != 127. || olo != 0. || ohi != 127.)
        u = (ihi > ilo) ? olo + (u - ilo) / (ihi - ilo) * (ohi - olo) : olo;
    }

    // Curve, compression and gain act on the distance to the rest value,
    // normalized so that 1 is the full range on that side.
    const double origin = bipolar ? 64. : 0.;
    const double span_up = bipolar ? 63. : 127.;
    const double span_down = bipolar ? 64. : 127.;
    double d = u - origin;
    const double sign = d < 0. ? -1. : 1.;
    double n = std::abs(d) / (d < 0. ? span_down : span_up);

    const double curve = std::clamp(double(in.curve.value), -1., 1.);
    if(curve != 0.)
      n = std::pow(std::min(n, 1.), std::pow(3., -curve));

    const double ratio = std::clamp(double(in.ratio.value), 0.25, 8.);
    const double thresh = std::clamp(double(in.threshold.value), 0., 127.) / 127.;
    if(ratio != 1. && n > thresh)
      n = thresh + (n - thresh) / ratio;

    n *= std::max(0., double(in.gain.value)) / 100. * key_gain(note);

    d = sign * n * (d < 0. ? span_down : span_up);
    u = origin + d;

    u += in.offset.value;

    const auto [rlo_f, rhi_f] = in.random.value;
    const double rlo = std::min(rlo_f, rhi_f), rhi = std::max(rlo_f, rhi_f);
    if(rhi > rlo)
      u += rlo + uniform() * (rhi - rlo);
    else
      u += rlo;

    const auto [clo_f, chi_f] = in.clip.value;
    const double clo = std::min(clo_f, chi_f), chi = std::max(clo_f, chi_f);
    u = std::clamp(u, clo, chi);

    return std::clamp(u, 0., 127.);
  }

  //! Gain of the key-tracked bands at a note, 1 when the value has no note.
  double key_gain(int note) const noexcept
  {
    if(note < 0)
      return 1.;
    const double lo = std::max(0., double(inputs.low_gain.value)) / 100.;
    const double mid = std::max(0., double(inputs.mid_gain.value)) / 100.;
    const double hi = std::max(0., double(inputs.high_gain.value)) / 100.;
    if(note <= 24)
      return lo;
    if(note <= 60)
      return lo + (mid - lo) * double(note - 24) / 36.;
    if(note <= 96)
      return mid + (hi - mid) * double(note - 60) / 36.;
    return hi;
  }

  //! 14-bit pitch bend to the 0 - 127 scale: 8192 is exactly 64.
  static double bend_to_scale(int v) noexcept
  {
    return v < 8192 ? double(v) / 8192. * 64. : 64. + double(v - 8192) / 8191. * 63.;
  }

  static int scale_to_bend(double u) noexcept
  {
    const double v = u < 64. ? u / 64. * 8192. : 8192. + (u - 64.) / 63. * 8191.;
    return std::clamp(int(std::lround(v)), 0, 16383);
  }

private:
  void process(libremidi::message& m) noexcept
  {
    if(!is_channel_message(m))
      return;
    if(inputs.channel != 0 && channel_of(m) != inputs.channel)
      return;

    const int number = inputs.number;
    const uint8_t status = m.bytes[0] & 0xF0;
    switch(inputs.kind.value)
    {
      case Velocity:
        if(is_note_on(m))
        {
          const double u = shape(m.bytes[2], m.bytes[1]);
          m.bytes[2] = uint8_t(std::clamp(int(std::lround(u)), 1, 127));
        }
        break;

      // A note-on with velocity 0 is a note-off without a release velocity:
      // giving it one would turn it into a note-on.
      case Release:
        if(status == 0x80 && m.size() >= 3)
          m.bytes[2] = to_7bit(shape(m.bytes[2], m.bytes[1]));
        break;

      case CC:
        if(status == 0xB0 && m.size() >= 3 && (number < 0 || m.bytes[1] == number)
           && !is_all_notes_off(m))
          m.bytes[2] = to_7bit(shape(m.bytes[2], -1));
        break;

      case PitchBend:
        if(status == 0xE0 && m.size() >= 3)
        {
          const int v = (m.bytes[2] << 7) | m.bytes[1];
          const int out = scale_to_bend(shape(bend_to_scale(v), -1));
          m.bytes[1] = uint8_t(out & 0x7F);
          m.bytes[2] = uint8_t((out >> 7) & 0x7F);
        }
        break;

      case Pressure:
        if(status == 0xD0 && m.size() >= 2)
          m.bytes[1] = to_7bit(shape(m.bytes[1], -1));
        break;

      case PolyPressure:
        if(status == 0xA0 && m.size() >= 3 && (number < 0 || m.bytes[1] == number))
          m.bytes[2] = to_7bit(shape(m.bytes[2], m.bytes[1]));
        break;
    }
  }

  static uint8_t to_7bit(double u) noexcept
  {
    return uint8_t(std::clamp(int(std::lround(u)), 0, 127));
  }

  double uniform() noexcept { return double(m_rng()) / 4294967296.0; }

  void reseed() noexcept
  {
    if(m_seed != 0)
      m_rng.seed(uint64_t(m_seed), UINT64_C(0x9e3779b97f4a7c15));
    else
      m_rng.seed(m_free_seed[0], m_free_seed[1]);
  }

public:
  struct ui
  {
    halp_meta(name, "Midi value shaper")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Select")
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::kind> kind;
      halp::item<&ins::number> number;
      halp::item<&ins::channel> channel;
    } select;

    struct
    {
      halp_meta(name, "Shape")
      halp_meta(layout, halp::layouts::tabs)
      halp_meta(background, halp::colors::background_darker)

      struct
      {
        halp_meta(name, "Range")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::fixed> fixed;
        struct
        {
          halp_meta(layout, halp::layouts::vbox)
          static constexpr auto condition()
          {
            return halp::enabled_when<&ins::fixed, true>{};
          }
          halp::item<&ins::fixed_value> fixed_value;
        } fixed_value;
        halp::item<&ins::in_range> in_range;
        halp::item<&ins::out_range> out_range;
      } range;

      struct
      {
        halp_meta(name, "Dynamics")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::curve> curve;
        halp::item<&ins::threshold> threshold;
        halp::item<&ins::ratio> ratio;
        halp::item<&ins::gain> gain;
        halp::item<&ins::offset> offset;
      } dynamics;

      struct
      {
        halp_meta(name, "Keys")
        halp_meta(layout, halp::layouts::hbox)
        halp::item<&ins::low_gain> low_gain;
        halp::item<&ins::mid_gain> mid_gain;
        halp::item<&ins::high_gain> high_gain;
      } keys;

      struct
      {
        halp_meta(name, "Limits")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::random> random;
        halp::item<&ins::seed> seed;
        halp::item<&ins::clip> clip;
      } limits;
    } shape;
  };

private:
  rnd::pcg m_rng;
  uint64_t m_free_seed[2]{};
  int64_t m_last_position{};
  int m_seed{};
  bool m_stop_requested{};
};
}
