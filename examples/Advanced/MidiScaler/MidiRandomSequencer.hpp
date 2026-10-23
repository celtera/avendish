#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/EventQueue.hpp>
#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <Advanced/MidiScaler/MidiCommon/Rhythm.hpp>
#include <Advanced/MidiScaler/MidiCommon/Theory.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/sample_accurate_controls.hpp>
#include <libremidi/message.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>

namespace mtk
{
/**
 * @brief A clocked random melody generator with two engines, after two
 * random-voltage Eurorack modules, translated to MIDI.
 *
 * Turing: after the Turing Machine by Tom Whitwell (Music Thing Modular, open
 * hardware, CC-BY-SA; https://musicthing.co.uk/pages/turing.html). A 16-bit
 * shift register turns once per step; the bit leaving position Length comes
 * back in at the bottom. Lock decides what happens to it: at 1 it is kept (a
 * locked loop of Length steps), at 0 it is replaced by a random bit (every
 * step new), in between it is replaced with probability 1 - Lock (the loop
 * mutates slowly). Below 0, like the big knob turned left of noon, it is
 * inverted with probability -Lock instead: at -1 the loop plays itself and
 * then its inverse, a locked loop twice as long. Write forces the incoming
 * bit. The lower 8 bits are the step's value (the module's DAC), and bits 1
 * to 4 can drive the gate, as on the Pulses expander.
 *
 * Marbles: after the X and T sections of Marbles by Emilie Gillet (Mutable
 * Instruments, firmware under the MIT licence;
 * https://pichenettes.github.io/mutable-instruments-documentation/modules/marbles/).
 * X draws a value per step: Spread 0 is the constant Bias, larger spreads
 * draw further from it, 1 covering the whole range (skewed towards Bias, like
 * Marbles' beta-shaped distributions). Deja vu is the probability of replaying
 * the draw made Length steps earlier instead of a new one: 0 always new, 1 a
 * locked loop. The loop stores the raw draws, so Spread, Bias and Steps still
 * shape a locked loop. The T section is reduced to a gate per step: Density
 * is the probability a step plays, Regularity moves from independent coin
 * tosses (0) to the same density laid out evenly over the loop (1).
 *
 * Both engines share the output stage. The value picks a note among the notes
 * of the scale within Range, from low to high. Steps above 0 keeps only the
 * strongest degrees (1: root and fifth), like Marbles' Steps knob past noon.
 * Octave jump adds up to that many octaves at random. Jitter delays a note by
 * up to half a step: it never plays early, so there is no added latency.
 *
 * Every random choice of a step comes from the engine's state (the register,
 * or the loop memory), so a locked loop repeats its pitches, gates,
 * velocities, accents and jitter exactly.
 *
 * The clock is the Rate grid (on the musical grid from the start of the
 * timeline when synced), the note-ons of the input (consumed with their
 * note-offs), or only the Step input. A Step impulse adds a step in any mode.
 * With a Seed, the same transport gives the same take; a rewind, a restart
 * of the transport or Reset starts it again.
 */
struct MidiRandomSequencer
{
  halp_meta(name, "Midi random sequencer")
  halp_meta(c_name, "midi_random_sequencer")
  halp_meta(category, "Midi/Generators")
  halp_meta(author, "ossia score")
  halp_meta(
      description, "Random melodies from a shift register (Turing Machine) or a "
                   "random distribution with loop memory (Marbles)")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-random-sequencer.html")
  halp_meta(uuid, "1fbb46dd-c422-40aa-8d0b-b9237ba91aa8")

  static constexpr int max_length = 16;
  static constexpr int max_steps_per_tick = 64;

  enum Engine
  {
    Turing,
    Marbles
  };

  enum Write
  {
    Off,
    Ones,
    Zeros
  };

  struct clock_control
  {
    static clang_buggy_consteval auto name() { return std::string_view{"Clock"}; }
    halp_meta(
        description,
        "Rate: a step every Rate. Input notes: each incoming note-on is a step "
        "(the input notes are consumed). Step only: only the Step input advances.")
    enum enum_type
    {
      rate,
      input_notes,
      step_only
    } value{rate};
    enum widget
    {
      combobox
    };
    struct range
    {
      std::string_view values[3]{"Rate", "Input notes", "Step only"};
      enum_type init = rate;
    };
    operator enum_type&() noexcept { return value; }
    operator const enum_type&() const noexcept { return value; }
    auto& operator=(enum_type t) noexcept
    {
      value = t;
      return *this;
    }
  };

  struct gate_control
  {
    static clang_buggy_consteval auto name() { return std::string_view{"Gate"}; }
    halp_meta(
        description,
        "Which steps play in Turing mode: every step, the steps where a register "
        "bit is set (like the Pulses expander), or Density as in Marbles mode")
    enum enum_type
    {
      every,
      bit1,
      bit2,
      bit3,
      bit4,
      bit1_or_2,
      bit1_and_2,
      density
    } value{every};
    enum widget
    {
      combobox
    };
    struct range
    {
      std::string_view values[8]{"Every step", "Bit 1",       "Bit 2",        "Bit 3",
                                 "Bit 4",      "Bit 1 or 2", "Bit 1 and 2", "Density"};
      enum_type init = every;
    };
    operator enum_type&() noexcept { return value; }
    operator const enum_type&() const noexcept { return value; }
    auto& operator=(enum_type t) noexcept
    {
      value = t;
      return *this;
    }
  };

  struct root_control
  {
    static clang_buggy_consteval auto name() { return std::string_view{"Root"}; }
    halp_meta(description, "Tonic of the scale")
    enum enum_type
    {
      c,
      cs,
      d,
      ds,
      e,
      f,
      fs,
      g,
      gs,
      a,
      as,
      b
    } value{c};
    enum widget
    {
      combobox
    };
    struct range
    {
      std::string_view values[12]{"C",  "C#", "D",  "D#", "E",  "F",
                                  "F#", "G",  "G#", "A",  "A#", "B"};
      enum_type init = c;
    };
    operator enum_type&() noexcept { return value; }
    operator const enum_type&() const noexcept { return value; }
    auto& operator=(enum_type t) noexcept
    {
      value = t;
      return *this;
    }
  };

  struct scale_control
  {
    static clang_buggy_consteval auto name() { return std::string_view{"Scale"}; }
    halp_meta(description, "Notes the sequence is quantized to")
    enum enum_type
    {
      major,
      dorian,
      phrygian,
      lydian,
      mixolydian,
      minor,
      locrian,
      harmonic_minor,
      melodic_minor,
      major_pentatonic,
      minor_pentatonic,
      chromatic
    } value{major};
    enum widget
    {
      combobox
    };
    struct range
    {
      std::string_view values[12]{
          "Major",          "Dorian",         "Phrygian",         "Lydian",
          "Mixolydian",     "Minor",          "Locrian",          "Harmonic minor",
          "Melodic minor",  "Major pentatonic", "Minor pentatonic", "Chromatic"};
      enum_type init = major;
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

    struct : halp::enum_t<Engine, "Engine">
    {
      enum widget
      {
        combobox
      };
      halp_meta(
          description, "Turing: a looping shift register. Marbles: random draws "
                       "with a loop memory.")
    } engine;

    clock_control clock;

    struct : halp::time_chooser<"Rate", halp::range{0.005, 4., 0.125}>
    {
      halp_meta(
          description, "Length of a step; when synced, the steps follow the "
                       "musical grid from the start of the timeline")
    } rate;

    struct : halp::impulse_button<"Step">
    {
      halp_meta(description, "Plays one step now, in any clock mode")
    } step;

    struct : halp::impulse_button<"Reset">
    {
      halp_meta(
          description, "Starts the take again: with a seed the same sequence, "
                       "without a new one")
    } reset;

    struct : halp::knob_f32<"Swing", halp::range{0., 1., 0.}>
    {
      halp_meta(
          description, "Delays every second step: 0.5 puts it on the triplet")
    } swing;

    struct : halp::spinbox_i32<"Length", halp::irange{1, max_length, 8}>
    {
      halp_meta(
          description, "Steps in the loop: the register length in Turing mode, "
                       "the loop memory in Marbles mode")
    } length;

    struct : halp::knob_f32<"Lock", halp::range{-1., 1., 0.9}>
    {
      halp_meta(
          description,
          "1: a locked loop. 0: every step random. In between the loop mutates, "
          "slower towards 1. Below 0 the bits get inverted: -1 is a locked loop "
          "followed by its inverse, twice as long.")
    } lock;

    struct : halp::enum_t<Write, "Write">
    {
      enum widget
      {
        combobox
      };
      halp_meta(
          description, "Forces the bit entering the register: Ones raises the "
                       "values, Zeros lowers them")
    } write;

    gate_control gate;

    struct : halp::knob_f32<"Spread", halp::range{0., 1., 0.5}>
    {
      halp_meta(
          description, "0: always the Bias value. 1: anywhere in the range, "
                       "most often near Bias.")
    } spread;

    struct : halp::knob_f32<"Bias", halp::range{0., 1., 0.5}>
    {
      halp_meta(description, "Centre of the random values: low to high notes")
    } bias;

    struct : halp::knob_f32<"Deja vu", halp::range{0., 1., 0.}>
    {
      halp_meta(
          description, "Probability of replaying the step of the previous loop "
                       "instead of a new draw: 1 is a locked loop")
    } deja_vu;

    struct : halp::knob_f32<"Density", halp::range{0., 1., 1.}>
    {
      halp_meta(description, "Probability that a step plays a note")
    } density;

    struct : halp::knob_f32<"Regularity", halp::range{0., 1., 0.}>
    {
      halp_meta(
          description, "0: each step tosses a coin. 1: the same number of notes "
                       "spread evenly over the loop.")
    } regularity;

    struct : halp::knob_f32<"Jitter", halp::range{0., 1., 0.}>
    {
      halp_meta(
          description, "Random delay of each note, up to half a step at 1; notes "
                       "never play early")
    } jitter;

    root_control root;
    scale_control key_mode;

    struct : halp::knob_f32<"Steps", halp::range{0., 1., 0.}>
    {
      halp_meta(
          description, "0: any note of the scale. Towards 1 only the strongest "
                       "degrees remain: at 1 the root and the fifth.")
    } steps;

    struct : halp::range_slider_f32<
                 "Range", halp::range_slider_range{0., 127., {48., 72.}}>
    {
      halp_meta(description, "Lowest and highest notes, as MIDI note numbers")
    } note_range;

    struct : halp::spinbox_i32<"Octave jump", halp::irange{0, 4, 0}>
    {
      halp_meta(description, "Each note may jump up by as many octaves, at random")
    } octaves;

    struct : halp::hslider_i32<"Gate length", halp::irange{1, 200, 50}>
    {
      halp_meta(
          description, "Length of a note in percent of a step; above 100 the "
                       "notes overlap")
    } gate_length;

    struct : halp::range_slider_f32<
                 "Velocity", halp::range_slider_range{1., 127., {80., 110.}}>
    {
      halp_meta(description, "Velocities are drawn within this range")
    } velocity;

    struct : halp::toggle<"Accent">
    {
      halp_meta(
          description,
          "Accented steps (register bit 5 in Turing mode, one step in four on "
          "average in Marbles mode) play at the highest velocity, the others in "
          "the lower half of the range")
    } accent;

    struct : halp::spinbox_i32<"Channel", halp::irange{1, 16, 1}>
    {
      halp_meta(description, "MIDI channel of the notes")
    } channel;

    struct : halp::spinbox_i32<"Seed", halp::irange{0, 10000, 0}>
    {
      halp_meta(
          description, "0 runs freely; any other value gives a reproducible take, "
                       "restarted when the transport rewinds")
    } seed;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;

    struct : halp::accurate<halp::val_port_01<"Value", float>>
    {
      halp_meta(
          description, "Value of each step, 0 to 1, before quantization: the "
                       "register in Turing mode, X in Marbles mode")
    } value;

    struct : halp::accurate<halp::val_port<"Gate", bool>>
    {
      halp_meta(description, "True while a note of the sequence sounds")
    } gate;
  } outputs;

  struct ui
  {
    halp_meta(name, "Midi random sequencer")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Clock")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::engine> engine;
      halp::item<&ins::clock> clock;
      halp::item<&ins::rate> rate;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::clock, clock_control::rate>{};
        }
        halp::item<&ins::swing> swing;
      } swing;
      halp::item<&ins::length> length;
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::item<&ins::step> step;
        halp::item<&ins::reset> reset;
      } buttons;
      halp::item<&ins::seed> seed;
    } clock;

    struct
    {
      halp_meta(name, "Turing")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::engine, Turing>{};
        }
        halp::item<&ins::lock> lock;
        halp::item<&ins::write> write;
        halp::item<&ins::gate> gate;
      } turing;
    } turing;

    struct
    {
      halp_meta(name, "Marbles")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::engine, Marbles>{};
        }
        halp::item<&ins::spread> spread;
        halp::item<&ins::bias> bias;
        halp::item<&ins::deja_vu> deja_vu;
      } marbles;
    } marbles;

    struct
    {
      halp_meta(name, "Rhythm")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::density> density;
      halp::item<&ins::regularity> regularity;
      halp::item<&ins::jitter> jitter;
    } rhythm;

    struct
    {
      halp_meta(name, "Pitch")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::root> root;
      halp::item<&ins::key_mode> key_mode;
      halp::item<&ins::steps> steps;
      halp::item<&ins::note_range> note_range;
      halp::item<&ins::octaves> octaves;
    } pitch;

    struct
    {
      halp_meta(name, "Notes")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::gate_length> gate_length;
      halp::item<&ins::velocity> velocity;
      halp::item<&ins::accent> accent;
      halp::item<&ins::channel> channel;
    } notes;
  };

  //! std::random_device may allocate, block or throw: it is read here, once,
  //! and never on the audio thread.
  MidiRandomSequencer()
  {
    std::random_device rd;
    m_rng.state = (uint64_t(rd()) << 32) ^ uint64_t(rd());
    init_memory();
  }

  void prepare(halp::setup s) noexcept
  {
    if(s.rate > 0.)
      m_rate = s.rate;
  }

  //! The next tick, possibly of 0 frames, ends every note.
  void stop() noexcept { m_stop_requested = true; }

  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& t) noexcept
  {
    const int frames = t.frames > 0 ? t.frames : 0;
    m_frames = frames;
    read_parameters();

    if(m_stop_requested)
    {
      m_stop_requested = false;
      end_all(0, 0);
      m_held = {};
      m_running = false;
    }

    if(frames <= 0)
      return;

    if(!m_running)
    {
      m_running = true;
      restart_take();
    }
    else if(t.position_in_frames < m_last_position)
    {
      // A jump back: the grid starts again, and a seeded take with it
      if(m_seed != 0)
      {
        end_all(0, 0);
        restart_take();
      }
      else
      {
        restart_clock();
      }
    }
    m_last_position = t.position_in_frames;

    if(inputs.reset.value)
      restart_take();

    plan_steps(t);

    for(const auto& m : inputs.midi)
    {
      const int ts = std::clamp<int>(int(m.timestamp), 0, frames - 1);
      advance(ts);
      handle(m, ts);
    }
    advance(frames);
    m_now += frames;
  }

  //! Notes of the sequence sounding now.
  int sounding() const noexcept { return m_total; }

  //! The register, for tests and displays.
  uint16_t register_bits() const noexcept { return m_register; }

private:
  // ---------------------------------------------------------------- random

  //! SplitMix64: one 64-bit state, no allocation, the same on every platform.
  struct rng
  {
    uint64_t state{};
    uint64_t next() noexcept
    {
      uint64_t z = (state += 0x9E3779B97F4A7C15ull);
      z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
      z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
      return z ^ (z >> 31);
    }
    double uniform() noexcept { return double(next() >> 11) * 0x1.0p-53; }
  };

  static double hash01(uint64_t v) noexcept
  {
    rng r{v * 0xD1B54A32D192ED03ull};
    return r.uniform();
  }

  //! The random parts of a step, as uniform draws in [0, 1).
  struct draws
  {
    double x{};
    double gate{};
    double jitter{};
    double velocity{};
    double accent{};
    double octave{};
  };

  draws fresh_draws() noexcept
  {
    draws d;
    d.x = m_rng.uniform();
    d.gate = m_rng.uniform();
    d.jitter = m_rng.uniform();
    d.velocity = m_rng.uniform();
    d.accent = m_rng.uniform();
    d.octave = m_rng.uniform();
    return d;
  }

  void init_memory() noexcept
  {
    for(auto& d : m_memory)
      d = fresh_draws();
    // A register whose bits repeat with the loop length: a locked loop is
    // periodic from its first step
    const int len = std::clamp(m_length, 1, max_length);
    uint16_t pattern = 0;
    for(int i = 0; i < len; i++)
      if(m_rng.uniform() < 0.5)
        pattern |= uint16_t(1u << i);
    m_register = 0;
    for(int i = 0; i < 16; i++)
      if(pattern & (1u << (i % len)))
        m_register |= uint16_t(1u << i);
  }

  void restart_clock() noexcept
  {
    m_last_q = -std::numeric_limits<double>::infinity();
    m_free_next = -1.;
    m_free_index = 0;
  }

  void restart_take() noexcept
  {
    if(m_seed != 0)
      m_rng.state = uint64_t(m_seed) * 0x2545F4914F6CDD1Dull;
    m_step = 0;
    init_memory();
    restart_clock();
  }

  // ---------------------------------------------------------------- engines

  struct step_result
  {
    double value{};
    bool gate{};
    double jitter{};
    double velocity{};
    bool accent{};
    double octave{};
  };

  bool density_gate(double draw, int pos) const noexcept
  {
    const int pulses = int(std::lround(m_density * m_length));
    const bool even = euclid_hit(pos, m_length, pulses, 0);
    const double r = m_regularity;
    return draw * (1. - r) + (even ? 0. : 1.) * r < m_density;
  }

  step_result turing_step() noexcept
  {
    const double change = m_rng.uniform();
    const double coin = m_rng.uniform();
    const int len = m_length;
    const bool out = (m_register >> (len - 1)) & 1u;
    bool in{};
    switch(m_write)
    {
      case Ones:
        in = true;
        break;
      case Zeros:
        in = false;
        break;
      default:
        if(m_lock >= 0.)
          in = change < 1. - m_lock ? coin < 0.5 : out;
        else
          in = change < 1. + m_lock ? coin < 0.5 : !out;
        break;
    }
    m_register = uint16_t((m_register << 1) | (in ? 1u : 0u));

    const uint16_t r = m_register;
    auto bit = [r](int n) { return ((r >> (n - 1)) & 1u) != 0; };
    step_result s;
    s.value = double(r & 0xFF) / 255.;
    s.jitter = hash01(uint64_t(r) | (1ull << 16));
    s.velocity = hash01(uint64_t(r) | (2ull << 16));
    s.octave = hash01(uint64_t(r) | (3ull << 16));
    s.accent = bit(5);
    switch(m_gate)
    {
      case gate_control::every:
        s.gate = true;
        break;
      case gate_control::bit1:
      case gate_control::bit2:
      case gate_control::bit3:
      case gate_control::bit4:
        s.gate = bit(int(m_gate) - int(gate_control::bit1) + 1);
        break;
      case gate_control::bit1_or_2:
        s.gate = bit(1) || bit(2);
        break;
      case gate_control::bit1_and_2:
        s.gate = bit(1) && bit(2);
        break;
      case gate_control::density:
        s.gate = density_gate(hash01(uint64_t(r) | (4ull << 16)), int(m_step % len));
        break;
    }
    return s;
  }

  //! Spread and Bias shape a uniform draw: Spread scales the distance to
  //! Bias, and a small spread also pulls the draws towards it.
  double shape_x(double u) const noexcept
  {
    const double c = m_bias;
    const double s = m_spread;
    if(s <= 0.)
      return c;
    const double v = 2. * u - 1.;
    const double p = 1. + 3. * (1. - s);
    const double m = std::pow(std::abs(v), p) * s;
    return v >= 0. ? c + m * (1. - c) : c - m * c;
  }

  step_result marbles_step() noexcept
  {
    const double replay = m_rng.uniform();
    const draws fresh = fresh_draws();
    const int pos = int(m_step % m_length);
    draws& slot = m_memory[pos];
    if(!(replay < m_deja_vu))
      slot = fresh;

    step_result s;
    s.value = std::clamp(shape_x(slot.x), 0., 1.);
    s.gate = density_gate(slot.gate, pos);
    s.jitter = slot.jitter;
    s.velocity = slot.velocity;
    s.accent = slot.accent < 0.25;
    s.octave = slot.octave;
    return s;
  }

  // ------------------------------------------------------------------ pitch

  //! The notes of the scale within the range that pass the Steps filter.
  void build_notes() noexcept
  {
    // How strong a degree is, by semitones above the root, for the scales
    // that are not seven-note modes
    static constexpr double semitone_weight[12]
        = {1.0, 0.1, 0.3, 0.7, 0.7, 0.4, 0.1, 0.9, 0.2, 0.2, 0.5, 0.5};
    // By degree for the modes: root, fifth, third, seventh, fourth, second, sixth
    static constexpr double degree_weight[7] = {1.0, 0.3, 0.7, 0.4, 0.9, 0.2, 0.5};

    std::array<double, 12> weight{};
    weight.fill(-1.);
    const int k = int(m_scale);
    if(k <= int(scale_control::melodic_minor))
    {
      for(int d = 0; d < 7; d++)
        weight[mode_steps[k][d]] = degree_weight[d];
    }
    else if(k == scale_control::major_pentatonic)
    {
      for(int st : {0, 2, 4, 7, 9})
        weight[st] = semitone_weight[st];
    }
    else if(k == scale_control::minor_pentatonic)
    {
      for(int st : {0, 3, 5, 7, 10})
        weight[st] = semitone_weight[st];
    }
    else
    {
      for(int st = 0; st < 12; st++)
        weight[st] = semitone_weight[st];
    }

    const double threshold = m_steps * 0.9 - 1e-9;
    auto collect = [&](double min_weight) {
      m_note_count = 0;
      for(int n = m_low; n <= m_high; n++)
      {
        const double w = weight[(n - m_root + 120) % 12];
        if(w >= 0. && w >= min_weight)
          m_notes[m_note_count++] = uint8_t(n);
      }
    };
    collect(threshold);
    // A range too narrow for the filter falls back to the scale, then to
    // every note
    if(m_note_count == 0)
      collect(0.);
    if(m_note_count == 0)
    {
      for(int n = m_low; n <= m_high; n++)
        m_notes[m_note_count++] = uint8_t(n);
    }
  }

  int pitch_of(const step_result& s) const noexcept
  {
    int idx = int(s.value * m_note_count);
    idx = std::clamp(idx, 0, m_note_count - 1);
    int p = m_notes[idx];
    const int jump = std::min(int(s.octave * (m_octaves + 1)), m_octaves);
    p += 12 * jump;
    while(p > 127)
      p -= 12;
    return p;
  }

  int velocity_of(const step_result& s) const noexcept
  {
    double lo = m_vel_lo, hi = m_vel_hi;
    if(m_accent)
    {
      if(s.accent)
        return int(std::lround(hi));
      hi = lo + (hi - lo) * 0.5;
    }
    return std::clamp(int(std::lround(lo + s.velocity * (hi - lo))), 1, 127);
  }

  // ------------------------------------------------------------------- clock

  void read_parameters() noexcept
  {
    m_engine = inputs.engine.value;
    m_clock = inputs.clock.value;
    m_seed = std::clamp(inputs.seed.value, 0, 10000);
    m_length = std::clamp(inputs.length.value, 1, max_length);
    m_lock = std::clamp(double(inputs.lock.value), -1., 1.);
    m_write = inputs.write.value;
    m_gate = inputs.gate.value;
    m_spread = std::clamp(double(inputs.spread.value), 0., 1.);
    m_bias = std::clamp(double(inputs.bias.value), 0., 1.);
    m_deja_vu = std::clamp(double(inputs.deja_vu.value), 0., 1.);
    m_density = std::clamp(double(inputs.density.value), 0., 1.);
    m_regularity = std::clamp(double(inputs.regularity.value), 0., 1.);
    m_jitter = std::clamp(double(inputs.jitter.value), 0., 1.);
    m_swing = std::clamp(double(inputs.swing.value), 0., 1.);
    m_root = std::clamp(int(inputs.root.value), 0, 11);
    m_scale = inputs.key_mode.value;
    m_steps = std::clamp(double(inputs.steps.value), 0., 1.);
    m_low = std::clamp(int(std::lround(inputs.note_range.value.start)), 0, 127);
    m_high = std::clamp(int(std::lround(inputs.note_range.value.end)), 0, 127);
    if(m_low > m_high)
      std::swap(m_low, m_high);
    m_octaves = std::clamp(inputs.octaves.value, 0, 4);
    m_gate_length = std::clamp(inputs.gate_length.value, 1, 200);
    m_vel_lo = std::clamp(double(inputs.velocity.value.start), 1., 127.);
    m_vel_hi = std::clamp(double(inputs.velocity.value.end), 1., 127.);
    if(m_vel_lo > m_vel_hi)
      std::swap(m_vel_lo, m_vel_hi);
    m_accent = inputs.accent.value;
    m_channel = std::clamp(inputs.channel.value, 1, 16);
    m_step_frames = std::max(1., double(inputs.rate.value) * m_rate);
    build_notes();
  }

  void plan(int offset, int64_t index) noexcept
  {
    if(m_plan_count < max_steps_per_tick)
      m_plan[m_plan_count++] = {offset, index};
  }

  //! The steps of this tick, in order, as frame offsets.
  void plan_steps(const halp::tick_musical& t) noexcept
  {
    m_plan_count = 0;
    m_plan_next = 0;

    if(inputs.step.value)
      plan(0, m_manual_index++);

    if(m_clock != clock_control::rate)
      return;

    const int frames = m_frames;
    const double s = t.start_position_in_quarters;
    const double e = t.end_position_in_quarters;
    const double tempo = t.tempo > 0. ? t.tempo : 120.;
    if(inputs.rate.sync && e > s)
    {
      // The step in quarters, rounded to 1/1920 of a quarter so that the
      // seconds the host hands over land back on the exact note value
      const double len_q
          = std::round(double(inputs.rate.value) * tempo / 60. * 1920.) / 1920.;
      if(len_q <= 0.)
        return;
      constexpr double eps = 1e-9;
      int64_t k = int64_t(std::ceil(s / len_q - eps));
      for(; double(k) * len_q < e - eps; k++)
      {
        const double q = double(k) * len_q;
        if(q <= m_last_q + eps)
          continue;
        m_last_q = q;
        const int off = std::clamp(
            int(std::llround((q - s) / (e - s) * frames)), 0, frames - 1);
        plan(off, k);
        if(m_plan_count == max_steps_per_tick)
          break;
      }
      m_free_next = -1.;
      return;
    }

    if(m_free_next < 0.)
      m_free_next = double(m_now);
    while(m_free_next < double(m_now + frames) && m_plan_count < max_steps_per_tick)
    {
      const int off
          = std::clamp(int(std::floor(m_free_next - double(m_now))), 0, frames - 1);
      plan(off, m_free_index++);
      m_free_next += m_step_frames;
    }
    // A step too short for the tick drops the excess instead of piling up
    if(m_free_next < double(m_now + frames))
      m_free_next = double(m_now + frames);
  }

  //! Plays the planned steps before `ts`, then sends what is due before it.
  void advance(int ts) noexcept
  {
    while(m_plan_next < m_plan_count && m_plan[m_plan_next].offset < ts)
    {
      const auto& p = m_plan[m_plan_next++];
      fire_step(p.offset, p.index, m_clock == clock_control::rate);
    }
    m_queue.release(m_now, ts, [this](libremidi::message&& m, uint32_t tag) {
      send(std::move(m), tag);
    });
  }

  void fire_step(int offset, int64_t index, bool swung) noexcept
  {
    const step_result s = m_engine == Marbles ? marbles_step() : turing_step();
    m_step++;

    const double sw = swung ? swing_delay(index, m_swing) : 0.;
    const int64_t at = m_now + offset + int64_t(std::llround(sw * m_step_frames));

    uint32_t bits = std::bit_cast<uint32_t>(float(s.value));
    m_queue.schedule(at, value_marker(), bits);

    if(!s.gate)
      return;
    const int64_t on_at
        = at + int64_t(std::llround(s.jitter * m_jitter * 0.5 * m_step_frames));
    const int64_t len
        = std::max<int64_t>(1, std::llround(m_step_frames * m_gate_length / 100.));
    const int p = pitch_of(s);
    const int v = velocity_of(s);
    m_queue.schedule(on_at, libremidi::channel_events::note_on(m_channel, p, v));
    m_queue.schedule(on_at + len, libremidi::channel_events::note_off(m_channel, p, 0));
  }

  //! A placeholder in the queue for a change of the Value output, carried in
  //! the tag. 0xF4 is undefined in MIDI 1 and never leaves the object.
  static libremidi::message value_marker() noexcept { return libremidi::message{0xF4}; }

  // ----------------------------------------------------------------- output

  void push(libremidi::message m, int ts) noexcept
  {
    m.timestamp = ts;
    outputs.midi.midi_messages.push_back(std::move(m));
  }

  template <typename Port, typename V>
  static void write_timed(Port& port, int ts, V v) noexcept
  {
    // The map keeps 16 entries without allocating
    if(port.values.size() < 16 || port.values.find(ts) != port.values.end())
      port.values[ts] = v;
  }

  void send(libremidi::message&& m, uint32_t tag) noexcept
  {
    const int ts = int(m.timestamp);
    if(m.size() == 1 && m.bytes[0] == 0xF4)
    {
      write_timed(outputs.value, ts, std::bit_cast<float>(tag));
      return;
    }
    const int c = channel_of(m);
    const int p = m.bytes[1] & 127;
    auto& n = m_sounding[(c - 1) & 15][p];
    if(is_note_on(m))
    {
      // Overlapping notes of one pitch: the new one re-attacks, the pitch
      // ends with the last of them
      if(n > 0)
        push(libremidi::channel_events::note_off(c, p, 0), ts);
      else
        m_total++;
      n++;
      push(std::move(m), ts);
      write_timed(outputs.gate, ts, true);
    }
    else if(is_note_off(m))
    {
      if(n == 0)
        return;
      if(--n == 0)
      {
        m_total--;
        push(std::move(m), ts);
        if(m_total == 0)
          write_timed(outputs.gate, ts, false);
      }
    }
  }

  //! Ends what sounds and drops the queued notes, on one channel or (0) all.
  void end_all(int channel, int ts) noexcept
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
          m_total--;
          push(libremidi::channel_events::note_off(c, p, 0), ts);
        }
      }
    }
    m_queue.remove_if([&](const auto& e) {
      const int ch = channel_of(e.msg);
      return channel == 0 || (ch != 0 && ch == channel);
    });
    if(m_total == 0)
      write_timed(outputs.gate, ts, false);
  }

  // ------------------------------------------------------------------ input

  void handle(const libremidi::message& m, int ts) noexcept
  {
    switch(classify(m))
    {
      case midi_kind::note_on:
        if(m_clock == clock_control::input_notes)
        {
          m_held[(channel_of(m) - 1) & 15][m.bytes[1] & 127] = true;
          fire_step(ts, m_input_index++, false);
          return;
        }
        break;
      case midi_kind::note_off: {
        auto& h = m_held[(channel_of(m) - 1) & 15][m.bytes[1] & 127];
        if(h)
        {
          h = false;
          return;
        }
        break;
      }
      case midi_kind::control_change:
        if(is_all_notes_off(m))
          end_all(channel_of(m), ts);
        break;
      default:
        break;
    }
    push(m, ts);
  }

  struct planned
  {
    int offset{};
    int64_t index{};
  };

  std::array<draws, max_length> m_memory{};
  std::array<uint8_t, 128> m_notes{};
  std::array<planned, max_steps_per_tick> m_plan{};
  std::array<std::array<uint16_t, 128>, 16> m_sounding{};
  std::array<std::array<bool, 128>, 16> m_held{};
  event_queue<1024> m_queue;

  rng m_rng{};

  double m_rate{48000.};
  double m_step_frames{6000.};
  double m_last_q{-std::numeric_limits<double>::infinity()};
  double m_free_next{-1.};
  int64_t m_now{};
  int64_t m_last_position{};
  int64_t m_step{};
  int64_t m_free_index{};
  int64_t m_manual_index{};
  int64_t m_input_index{};
  int m_frames{};
  int m_plan_count{};
  int m_plan_next{};
  int m_note_count{};
  int m_total{};
  bool m_stop_requested{};
  bool m_running{};

  // Parameters, read at the start of each tick
  Engine m_engine{Turing};
  clock_control::enum_type m_clock{clock_control::rate};
  Write m_write{Off};
  gate_control::enum_type m_gate{gate_control::every};
  scale_control::enum_type m_scale{scale_control::major};
  uint16_t m_register{};
  int m_seed{};
  int m_length{8};
  int m_root{};
  int m_low{48}, m_high{72};
  int m_octaves{};
  int m_gate_length{50};
  int m_channel{1};
  double m_lock{0.9};
  double m_spread{0.5}, m_bias{0.5}, m_deja_vu{};
  double m_density{1.}, m_regularity{}, m_jitter{}, m_swing{};
  double m_steps{};
  double m_vel_lo{80.}, m_vel_hi{110.};
  bool m_accent{};
};
}
