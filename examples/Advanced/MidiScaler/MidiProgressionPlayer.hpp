#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>
#include <Advanced/MidiScaler/MidiCommon/Progressions.hpp>
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
#include <string>
#include <string_view>

namespace mtk
{
/**
 * @brief Plays a chord progression and keeps incoming notes in its harmony.
 *
 * The progression is one of the 194 of free-midi-chords (ProgressionTable.hpp)
 * or a custom text in the same roman-numeral notation (Progressions.hpp), in
 * a key. Each step plays the next chord, voiced near Register, each chord in
 * the inversion closest to the previous one (or in root position).
 *
 * Steps come from the musical grid (Every step), from the Next button, or from
 * each incoming note-on. On the grid, the chord is a function of the position:
 * step k of the timeline plays chord k modulo the length of the progression,
 * so that playing from anywhere, looping or rewinding gives the same chord at
 * the same place. A step shorter than a whole note restarts at every bar line
 * (a 3/4 bar in half notes has steps on beats 1 and 3); a whole note or more is
 * counted in bars from the last signature change, like the quantification
 * grid of score.
 *
 * Incoming notes pass, are snapped to the nearest tone of the current chord or
 * of the scale, or, with Keys play degrees, white keys from C4 play the chord's
 * tones upwards over octaves (C4 the root nearest C4, D4 the next tone...)
 * while black keys are silent. A note keeps the pitch it got at its note-on
 * until its note-off. Incoming notes on a rest follow the previous chord.
 *
 * Outputs give the current chord's name, its symbol as written, its index in
 * the progression and its root's pitch class (-1 on a rest), for other
 * processes to follow the harmony. A custom text that does not parse shows
 * the reason in Error and the last text that parsed keeps playing.
 *
 * Every note-on sent gets exactly one note-off: notes are counted per channel
 * and pitch, a note started twice is re-attacked and ends with the last of
 * them. All notes off (CC 120 / 123) ends what sounds on its channel and
 * passes; stop and rewind end everything. Note-offs that match no note-on
 * received are dropped. Other messages pass unchanged.
 */
struct MidiProgressionPlayer
{
  halp_meta(name, "Midi progression player")
  halp_meta(c_name, "midi_progression_player")
  halp_meta(category, "Midi/Generators")
  halp_meta(author, "ossia score")
  halp_meta(
      description, "Play a chord progression in a key, step by step, and keep "
                   "incoming notes in its harmony")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-progression-player.html")
  halp_meta(uuid, "213d75e7-3c2e-4b13-9686-fbfd2132fc2d")
  halp_flag(process_exec);

  static constexpr int max_held = 128;
  static constexpr int custom_index = 0;

  //! Index in the Progression list: 0 is Custom, i + 1 is progression_table[i].
  enum class progression_choice : uint16_t
  {
  };

  static constexpr auto progression_labels = [] {
    std::array<std::string_view, progression_count + 1> a{};
    a[0] = "Custom";
    for(int i = 0; i < progression_count; i++)
      a[i + 1] = progression_table[i].label;
    return a;
  }();

  static constexpr progression_choice default_progression = [] {
    for(int i = 0; i < progression_count; i++)
      if(progression_table[i].symbols == "I V vi IV")
        return progression_choice(i + 1);
    return progression_choice(1);
  }();

  enum class key_mode : uint8_t
  {
    automatic,
    major,
    minor,
    dorian,
    phrygian,
    lydian,
    mixolydian,
    locrian,
    harmonic_minor,
    melodic_minor
  };

  enum class advance_mode : uint8_t
  {
    every_step,
    on_trigger,
    on_note
  };

  enum class input_mode : uint8_t
  {
    pass,
    snap_chord,
    snap_scale,
    degrees
  };

  struct ins
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;

    // ---- Harmony ---------------------------------------------------------
    struct
    {
      static clang_buggy_consteval auto name() { return std::string_view{"Key"}; }
      halp_meta(description, "Root of the key")
      enum enum_type
      {
        C, Cs, D, Eb, E, F, Fs, G, Ab, A, Bb, B
      } value{C};
      enum widget
      {
        enumeration,
        list,
        combobox
      };
      struct range
      {
        std::string_view values[12]{"C",  "C#", "D",  "Eb", "E",  "F",
                                    "F#", "G",  "Ab", "A",  "Bb", "B"};
        enum_type init = C;
      };
      operator enum_type&() noexcept { return value; }
      operator const enum_type&() const noexcept { return value; }
      auto& operator=(enum_type t) noexcept
      {
        value = t;
        return *this;
      }
    } key;

    struct
    {
      static clang_buggy_consteval auto name() { return std::string_view{"Mode"}; }
      halp_meta(
          description,
          "Mode of the key: the chords of bare numerals (ii, V7) come from it. "
          "Auto: minor for the minor progressions of the list, major otherwise.")
      key_mode value{key_mode::automatic};
      enum widget
      {
        enumeration,
        list,
        combobox
      };
      struct range
      {
        std::string_view values[10]{
            "Auto",       "Major",      "Minor",          "Dorian",        "Phrygian",
            "Lydian",     "Mixolydian", "Locrian",        "Harmonic minor", "Melodic minor"};
        key_mode init = key_mode::automatic;
      };
      operator key_mode&() noexcept { return value; }
      operator const key_mode&() const noexcept { return value; }
      auto& operator=(key_mode t) noexcept
      {
        value = t;
        return *this;
      }
    } mode;

    struct
    {
      static clang_buggy_consteval auto name() { return std::string_view{"Progression"}; }
      halp_meta(
          description,
          "One of the 194 progressions of free-midi-chords, with their moods, or "
          "Custom to write one")
      progression_choice value{default_progression};
      enum widget
      {
        enumeration,
        list,
        combobox
      };
      struct range
      {
        std::array<std::string_view, progression_count + 1> values = progression_labels;
        progression_choice init = default_progression;
      };
      operator progression_choice&() noexcept { return value; }
      operator const progression_choice&() const noexcept { return value; }
      auto& operator=(progression_choice t) noexcept
      {
        value = t;
        return *this;
      }
    } progression;

    struct : halp::lineedit<"Custom", "I vi IV V">
    {
      halp_meta(
          description,
          "Roman numerals separated by spaces, e.g. ii7 V7 I7 or i bVIIM bVIM V7. "
          "Bare numerals take the chord of the key, 7 its seventh; suffixes M m "
          "dim dom7 M7 m7 sus2 sus4 6 9 add9 5 and more; b or # before the "
          "numeral; I/bVII for a bass; X or two spaces for a rest.")
    } custom;

    // ---- Advance ---------------------------------------------------------
    struct
    {
      static clang_buggy_consteval auto name() { return std::string_view{"Advance"}; }
      halp_meta(
          description,
          "Every step: on the musical grid. On trigger: at each Next. On note: at "
          "each incoming note-on.")
      advance_mode value{advance_mode::every_step};
      enum widget
      {
        enumeration,
        list,
        combobox
      };
      struct range
      {
        std::string_view values[3]{"Every step", "On trigger", "On note"};
        advance_mode init = advance_mode::every_step;
      };
      operator advance_mode&() noexcept { return value; }
      operator const advance_mode&() const noexcept { return value; }
      auto& operator=(advance_mode t) noexcept
      {
        value = t;
        return *this;
      }
    } advance;

    struct : halp::time_chooser<"Step", halp::range{0.05, 32., 2.}>
    {
      halp_meta(
          description,
          "Length of a chord. Synced: a note value on the musical grid (1 is a "
          "bar of 4/4, steps shorter than a whole note restart at each bar). "
          "Free: seconds from the start.")
    } step;

    struct : halp::knob_f32<"Gate", halp::range{0.01, 1., 1.}>
    {
      halp_meta(
          description, "Chord length as a proportion of the step; 1 holds it until "
                       "the next chord")
    } gate;

    struct : halp::impulse_button<"Next">
    {
      halp_meta(description, "Play the next chord (On trigger)")
    } next;

    struct : halp::toggle<"Consume", halp::toggle_setup{.init = true}>
    {
      halp_meta(
          description,
          "On note: the notes that step the progression are not passed on")
    } consume;

    // ---- Chords ----------------------------------------------------------
    struct : halp::toggle<"Chords", halp::toggle_setup{.init = true}>
    {
      halp_meta(description, "Play the chords; off, only the input is shaped")
    } chords;

    struct : halp::spinbox_i32<"Register", halp::irange{24, 108, 60}>
    {
      halp_meta(description, "Center note of the chords (60 is C4)")
    } center;

    struct
    {
      static clang_buggy_consteval auto name() { return std::string_view{"Inversion"}; }
      halp_meta(
          description,
          "Root: every chord in root position. Closest: the inversion that "
          "moves the voices the least from the previous chord.")
      inversion_mode value{inversion_mode::closest};
      enum widget
      {
        enumeration,
        list,
        combobox
      };
      struct range
      {
        std::string_view values[2]{"Root", "Closest"};
        inversion_mode init = inversion_mode::closest;
      };
      operator inversion_mode&() noexcept { return value; }
      operator const inversion_mode&() const noexcept { return value; }
      auto& operator=(inversion_mode t) noexcept
      {
        value = t;
        return *this;
      }
    } inversion;

    struct : halp::toggle<"Bass">
    {
      halp_meta(
          description, "Add the root (or the bass of a slash chord) an octave "
                       "below the register")
    } bass;

    struct : halp::spinbox_i32<"Notes", halp::irange{1, 6, 6}>
    {
      halp_meta(
          description, "Chord tones played at most; the fifth goes first, then "
                       "the extensions")
    } notes;

    struct : halp::hslider_i32<"Velocity", halp::irange{1, 127, 90}>
    {
      halp_meta(description, "Velocity of the chord notes")
    } velocity;

    struct : halp::spinbox_i32<"Channel", halp::irange{1, 16, 1}>
    {
      halp_meta(description, "MIDI channel of the chord notes")
    } channel;

    // ---- Input -----------------------------------------------------------
    struct
    {
      static clang_buggy_consteval auto name() { return std::string_view{"Input notes"}; }
      halp_meta(
          description,
          "Pass: unchanged. Snap to chord / scale: moved to the nearest tone of "
          "the current chord / of the key. Keys play degrees: white keys from C4 "
          "play the chord tones upwards, black keys are silent.")
      input_mode value{input_mode::pass};
      enum widget
      {
        enumeration,
        list,
        combobox
      };
      struct range
      {
        std::string_view values[4]{
            "Pass", "Snap to chord", "Snap to scale", "Keys play degrees"};
        input_mode init = input_mode::pass;
      };
      operator input_mode&() noexcept { return value; }
      operator const input_mode&() const noexcept { return value; }
      auto& operator=(input_mode t) noexcept
      {
        value = t;
        return *this;
      }
    } input;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;
    struct : halp::val_port<"Chord", std::string>
    {
      halp_meta(description, "Name of the current chord, e.g. Am7; empty on a rest")
    } chord;
    struct : halp::val_port<"Degree", std::string>
    {
      halp_meta(description, "Symbol of the current chord as written, e.g. vi7")
    } degree;
    struct : halp::val_port<"Index", int>
    {
      halp_meta(description, "Position of the current chord in the progression, from 0")
    } index;
    struct : halp::val_port<"Root", int>
    {
      halp_meta(description, "Pitch class of the current chord's root, 0 is C; -1 on a rest")
    } root;
    struct : halp::val_port<"Error", std::string>
    {
      halp_meta(description, "Why the custom progression does not parse; empty if it does")
    } error;
  } outputs;

  struct ui
  {
    halp_meta(name, "Midi progression player")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Harmony")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::item<&ins::key> key;
        halp::item<&ins::mode> mode;
      } key;
      halp::item<&ins::progression> progression;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          // An int: score matches enums by their enumerators, which this one has not
          return halp::enabled_when<&ins::progression, custom_index>{};
        }
        halp::item<&ins::custom> custom;
      } custom;
      halp::item<&ins::input> input;
    } harmony;

    struct
    {
      halp_meta(name, "Settings")
      halp_meta(layout, halp::layouts::tabs)
      halp_meta(background, halp::colors::background_darker)

      struct
      {
        halp_meta(name, "Advance")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::advance, halp::look{.widget = halp::control_widget::combo}>
            advance;
        struct
        {
          halp_meta(layout, halp::layouts::hbox)
          static constexpr auto condition()
          {
            return halp::enabled_when<&ins::advance, advance_mode::every_step>{};
          }
          halp::item<&ins::step> step;
          halp::item<&ins::gate> gate;
        } grid;
        struct
        {
          halp_meta(layout, halp::layouts::hbox)
          struct
          {
            halp_meta(layout, halp::layouts::vbox)
            static constexpr auto condition()
            {
              return halp::enabled_when<&ins::advance, advance_mode::on_trigger>{};
            }
            halp::item<&ins::next> next;
          } trigger;
          struct
          {
            halp_meta(layout, halp::layouts::vbox)
            static constexpr auto condition()
            {
              return halp::enabled_when<&ins::advance, advance_mode::on_note>{};
            }
            halp::item<&ins::consume> consume;
          } note;
        } manual;
      } advance;

      struct
      {
        halp_meta(name, "Chords")
        halp_meta(layout, halp::layouts::vbox)
        halp::item<&ins::chords> chords;
        struct
        {
          halp_meta(layout, halp::layouts::grid)
          halp_meta(columns, 2)
          static constexpr auto condition()
          {
            return halp::enabled_when<&ins::chords, true>{};
          }
          halp::item<&ins::center> center;
          halp::item<&ins::notes> notes;
          halp::item<&ins::inversion> inversion;
          halp::item<&ins::bass> bass;
          halp::item<&ins::velocity> velocity;
          halp::item<&ins::channel> channel;
        } voicing;
      } chords;
    } settings;
  };

  MidiProgressionPlayer() noexcept
  {
    outputs.chord.value.reserve(max_text);
    outputs.degree.value.reserve(max_text);
    outputs.error.value.reserve(max_text);
    outputs.index.value = -1;
    outputs.root.value = -1;
  }

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
    update_progression();

    if(m_stop_requested)
    {
      m_stop_requested = false;
      end_all(0, 0, true);
      reset();
    }

    // A rewind (or a loop) restarts the take
    const bool musical = m_frames > 0 && t.end_position_in_quarters != t.start_position_in_quarters;
    if(m_started
       && (t.position_in_frames < m_last_end
           || (musical && t.start_position_in_quarters < m_last_quarter - 1e-6)))
    {
      end_all(0, 0, false);
      reset();
    }
    m_last_end = t.position_in_frames + m_frames;
    if(musical)
      m_last_quarter = t.end_position_in_quarters;
    m_started = true;

    if(m_frames <= 0)
      return;

    const auto adv = inputs.advance.value;
    m_grid_count = 0;
    if(adv == advance_mode::every_step)
      compute_grid(t);
    int grid_i = 0;

    const auto run_until = [&](int64_t ts) {
      while(true)
      {
        const int64_t gate_ts = m_gate_end >= 0 ? m_gate_end - m_now : INT64_MAX;
        const int64_t grid_ts
            = grid_i < m_grid_count ? m_grid[grid_i].frame : INT64_MAX;
        const int64_t next = std::min(gate_ts, grid_ts);
        if(next > ts || next == INT64_MAX)
          break;
        if(gate_ts <= grid_ts)
        {
          m_gate_end = -1;
          end_chord(std::max<int64_t>(0, gate_ts));
        }
        else
        {
          const auto& g = m_grid[grid_i++];
          if(g.index != m_index)
            play_step(g.index, g.frame, g.length);
        }
      }
    };

    if(adv == advance_mode::on_trigger && inputs.next.value)
      play_step(m_index + 1, 0, -1);
    inputs.next.value.reset();

    for(const auto& m : inputs.midi)
    {
      const int64_t ts = std::clamp<int64_t>(m.timestamp, 0, m_frames - 1);
      run_until(ts);
      handle(m, ts);
    }
    run_until(m_frames - 1);
    m_now += m_frames;
  }

  //! Notes sounding now, for the tests.
  int sounding_notes() const noexcept
  {
    int n = 0;
    for(auto& c : m_sounding)
      for(auto v : c)
        n += v > 0;
    return n;
  }

  //! The parsed progression in use.
  const parsed_progression& progression() const noexcept { return m_prog; }

private:
  static constexpr std::size_t max_text = 256;

  struct grid_point
  {
    int64_t frame{};  //!< in the tick
    int64_t index{};  //!< step of the timeline
    int64_t length{}; //!< frames of the step, for the gate
  };

  struct held_note
  {
    uint8_t in_channel{};
    uint8_t in_pitch{};
    uint8_t out_channel{};
    uint8_t out_pitch{};
    bool silent{}; //!< consumed: neither its note-on nor its note-off goes out
  };

  // ------------------------------------------------------------- settings

  //! Re-parses when the choice or the custom text changes. Never allocates.
  void update_progression() noexcept
  {
    const int choice = std::clamp(int(inputs.progression.value), 0, progression_count);
    std::string_view text;
    if(choice == custom_index)
    {
      text = inputs.custom.value;
      if(text.size() > max_text)
        text = text.substr(0, max_text);
    }
    else
    {
      text = progression_table[choice - 1].symbols;
    }

    if(m_parsed_once && choice == m_choice
       && text == std::string_view{m_text_buf.data(), m_text_size})
      return;

    m_parsed_once = true;
    m_choice = choice;
    std::copy(text.begin(), text.end(), m_text_buf.begin());
    m_text_size = text.size();
    text = {m_text_buf.data(), m_text_size};

    parsed_progression p;
    const auto err = parse_progression(text, p);
    if(err)
    {
      std::array<char, max_text> buf{};
      const auto n = format_parse_error(err, text, buf);
      outputs.error.value.assign(buf.data(), n);
      return;
    }
    outputs.error.value.clear();
    m_prog = p;
    std::copy(text.begin(), text.end(), m_prog_text.begin());
    m_prog_size = text.size();
    m_family_minor
        = choice != custom_index
          && progression_table[choice - 1].family == progression_family::minor;
  }

  std::string_view prog_text() const noexcept { return {m_prog_text.data(), m_prog_size}; }

  int key_root() const noexcept { return std::clamp(int(inputs.key.value), 0, 11); }

  mode key_scale() const noexcept
  {
    switch(inputs.mode.value)
    {
      case key_mode::automatic:
        return m_family_minor ? mode::aeolian : mode::ionian;
      case key_mode::major:
        return mode::ionian;
      case key_mode::minor:
        return mode::aeolian;
      case key_mode::dorian:
        return mode::dorian;
      case key_mode::phrygian:
        return mode::phrygian;
      case key_mode::lydian:
        return mode::lydian;
      case key_mode::mixolydian:
        return mode::mixolydian;
      case key_mode::locrian:
        return mode::locrian;
      case key_mode::harmonic_minor:
        return mode::harmonic_minor;
      case key_mode::melodic_minor:
        return mode::melodic_minor;
    }
    return mode::ionian;
  }

  // ----------------------------------------------------------------- grid

  /**
   * Grid points of the tick, each with the step of the timeline it starts.
   * Synced, the frames are those of score's quantification grid
   * (get_quantification_date) and the step is computed from the musical
   * position; free, steps are counted in frames from the timeline's start.
   */
  void compute_grid(const halp::tick_musical& t) noexcept
  {
    const double tempo = t.tempo > 0. ? t.tempo : 120.;
    const double start_q = t.start_position_in_quarters;
    const double end_q = t.end_position_in_quarters;
    const bool musical = end_q > start_q;
    const double step_s = std::max(0.001, double(inputs.step.value));

    if(inputs.step.sync && musical)
    {
      // The binding converts the note value to seconds in float: back to
      // 1/1920 of a quarter, so that the grid is exact
      const double q = std::max(1. / 1920., std::round(step_s * tempo / 60. * 1920.) / 1920.);
      const double rate = 4. / q;
      const bool valid_sig = t.signature.num > 0 && t.signature.denom > 0;
      const double qib = valid_sig ? 4. * t.signature.num / t.signature.denom : 4.;
      const double origin = t.last_signature_change;
      const double qpf = (end_q - start_q) / m_frames;
      const double frames_per_q = 60. / tempo * m_rate;

      const int64_t per_bar = int64_t(std::ceil(qib / q - 1e-6));

      //! Step of the timeline at a musical position.
      const auto index_at = [&](double pos) -> int64_t {
        const double rel = pos - origin;
        if(rate <= 1.)
          return int64_t(std::floor(rel / (qib / rate) + 1e-6));
        const int64_t bar = int64_t(std::floor(rel / qib + 1e-6));
        const double within = rel - bar * qib;
        const int64_t k = int64_t(std::floor(within / q + 1e-6));
        return bar * per_bar + std::min(k, per_bar - 1);
      };

      //! Step a grid point starts. The point's frame can be a frame off its
      //! exact position (it is clamped into the tick that reports it), so the
      //! bar is the one where subdivision k falls nearest that frame.
      const auto point_index = [&](int f, int k) -> int64_t {
        if(rate <= 1.)
          return k;
        const double est = start_q + f * qpf - origin;
        const int64_t b0 = int64_t(std::floor(est / qib));
        int64_t best = b0;
        double best_d = 1e300;
        for(int64_t b = b0 - 1; b <= b0 + 1; b++)
        {
          const double d = std::abs(b * qib + k * q - est);
          if(d < best_d)
          {
            best_d = d;
            best = b;
          }
        }
        return best * per_bar + std::min<int64_t>(k, per_bar - 1);
      };
      const int64_t len = int64_t(std::llround((rate <= 1. ? qib / rate : q) * frames_per_q));

      const auto points = t.get_quantification_date(rate);
      if(m_index < 0 && (points.empty() || points.front().first != 0))
        push_grid(0, index_at(start_q), len);
      for(const auto& [f, k] : points)
        push_grid(f, point_index(f, k), len);
    }
    else
    {
      const int64_t step = std::max<int64_t>(1, std::llround(step_s * m_rate));
      const int64_t pos = t.position_in_frames;
      if(m_index < 0)
        push_grid(0, pos / step, step);
      int64_t next = ((pos + step - 1) / step) * step;
      for(; next < pos + m_frames; next += step)
        push_grid(next - pos, next / step, step);
    }
  }

  void push_grid(int64_t frame, int64_t index, int64_t length) noexcept
  {
    if(m_grid_count > 0 && m_grid[m_grid_count - 1].index == index)
      return;
    if(m_grid_count < int(m_grid.size()))
      m_grid[m_grid_count++] = {frame, index, length};
  }

  // ----------------------------------------------------------------- steps

  //! Plays chord `step` (modulo the progression's length) at `ts`; a step
  //! length >= 0 starts the gate.
  void play_step(int64_t step, int64_t ts, int64_t length) noexcept
  {
    end_chord(ts);
    m_gate_end = -1;
    m_index = step;
    if(m_prog.count <= 0)
      return;
    const int i = int(((step % m_prog.count) + m_prog.count) % m_prog.count);
    const auto& sym = m_prog.chords[i];
    const auto chord = realize(sym, key_root(), key_scale());

    if(!chord.rest)
    {
      m_harmony = chord;
      m_has_harmony = true;
      if(inputs.chords.value)
      {
        voicing_options o;
        o.center = inputs.center.value;
        o.inversion = inputs.inversion.value;
        o.bass = inputs.bass.value;
        o.max_notes = inputs.notes.value;
        m_voicing = voice(chord, m_previous, o);
        m_previous = m_voicing;
        m_chord_channel = std::clamp(inputs.channel.value, 1, 16);
        const int vel = std::clamp(inputs.velocity.value, 1, 127);
        for(int p : m_voicing.all())
          send_on(m_chord_channel, p, vel, ts);
        // Within half a percent of 1 the knob means "until the next chord"
        const double gate = std::clamp(double(inputs.gate.value), 0.01, 1.);
        if(length > 0 && gate < 0.995)
          m_gate_end
              = m_now + ts + std::max<int64_t>(1, std::llround(double(length) * gate));
      }
    }

    // Outputs
    std::array<char, 32> buf{};
    const bool flats = sym.accidental != 0 ? sym.accidental < 0
                                           : key_prefers_flats(key_root(), key_scale());
    const auto n = format_chord_name(chord, flats, buf);
    assign(outputs.chord.value, {buf.data(), n});
    assign(outputs.degree.value, sym.rest ? std::string_view{"X"} : sym.text(prog_text()));
    outputs.index.value = i;
    outputs.root.value = chord.rest ? -1 : chord.root;
  }

  static void assign(std::string& s, std::string_view v) noexcept
  {
    if(v.size() > max_text)
      v = v.substr(0, max_text);
    if(s != v)
      s.assign(v.data(), v.size());
  }

  void end_chord(int64_t ts) noexcept
  {
    for(int p : m_voicing.all())
      send_off(m_chord_channel, p, 0, ts);
    m_voicing = {};
  }

  //! The chord that input notes follow: the last one played, else the first
  //! of the progression.
  const realized_chord& harmony() noexcept
  {
    if(!m_has_harmony)
    {
      m_fallback = {};
      for(const auto& s : m_prog.symbols())
        if(!s.rest)
        {
          m_fallback = realize(s, key_root(), key_scale());
          break;
        }
      return m_fallback;
    }
    return m_harmony;
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
          note_off(channel_of(m), m.bytes[1] & 127, m.bytes[2] & 127, ts);
          return;
        }
        break;
      case midi_kind::control_change:
        if(is_all_notes_off(m))
        {
          end_all(channel_of(m), ts, false);
          push(m, ts);
          return;
        }
        break;
      default:
        break;
    }
    push(m, ts);
  }

  void note_on(int c, int p, int v, int64_t ts) noexcept
  {
    bool silent = false;
    if(inputs.advance.value == advance_mode::on_note)
    {
      play_step(m_index + 1, ts, -1);
      silent = inputs.consume.value;
    }

    int out = p;
    if(!silent)
    {
      switch(inputs.input.value)
      {
        case input_mode::pass:
          break;
        case input_mode::snap_chord:
          out = nearest_chord_tone(p, harmony());
          break;
        case input_mode::snap_scale:
          out = nearest_scale_tone(p, key_root(), key_scale());
          break;
        case input_mode::degrees: {
          constexpr int white[12]{0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
          const int w = white[p % 12];
          if(w < 0)
          {
            silent = true;
            break;
          }
          const auto& h = harmony();
          if(h.rest)
          {
            silent = true;
            break;
          }
          const int root_pitch = 60 + ((h.root + 6) % 12) - 6;
          out = chord_tone(h, root_pitch, (p / 12) * 7 + w - 35);
          if(out < 0 || out > 127)
            silent = true;
          break;
        }
      }
    }

    if(m_held_count == max_held)
      release_held(0, ts, 0);
    held_note& h = m_held[m_held_count++];
    h = {uint8_t(c), uint8_t(p), uint8_t(c), uint8_t(std::clamp(out, 0, 127)), silent};
    if(!silent)
      send_on(c, h.out_pitch, v, ts);
  }

  //! A note-off that matches no note-on seen is dropped: every note this
  //! object sends is tracked, so such a note-off could only end one of them
  //! (a chord note) or end nothing.
  void note_off(int c, int p, int vel, int64_t ts) noexcept
  {
    for(int i = 0; i < m_held_count; i++)
      if(m_held[i].in_channel == c && m_held[i].in_pitch == p)
      {
        release_held(i, ts, vel);
        return;
      }
  }

  void release_held(int i, int64_t ts, int vel) noexcept
  {
    const held_note h = m_held[i];
    if(!h.silent)
      send_off(h.out_channel, h.out_pitch, vel, ts);
    for(int k = i; k + 1 < m_held_count; k++)
      m_held[k] = m_held[k + 1];
    m_held_count--;
  }

  // ---------------------------------------------------------------- output

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

  //! Ends what sounds, on one channel or (0) all. The held input notes are
  //! forgotten, or kept silent so that their note-offs do not go out alone.
  void end_all(int channel, int64_t ts, bool forget) noexcept
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
    if(channel == 0 || channel == m_chord_channel)
    {
      m_voicing = {};
      m_gate_end = -1;
    }
    if(forget)
    {
      m_held_count = 0;
      return;
    }
    for(int i = 0; i < m_held_count; i++)
      if(channel == 0 || m_held[i].out_channel == channel)
        m_held[i].silent = true;
  }

  //! Back to before the first step: the next one starts the progression.
  void reset() noexcept
  {
    m_index = -1;
    m_previous = {};
    m_voicing = {};
    m_gate_end = -1;
    m_has_harmony = false;
    m_started = false;
    m_last_quarter = -1e300;
  }

  parsed_progression m_prog{};
  std::array<char, max_text> m_prog_text{};
  std::size_t m_prog_size{};
  std::array<char, max_text> m_text_buf{};
  std::size_t m_text_size{};
  int m_choice{-1};
  bool m_parsed_once{};
  bool m_family_minor{};

  std::array<grid_point, 64> m_grid{};
  int m_grid_count{};

  int64_t m_index{-1};
  realized_chord m_harmony{};
  realized_chord m_fallback{};
  bool m_has_harmony{};
  voicing m_voicing{};
  voicing m_previous{};
  int m_chord_channel{1};
  int64_t m_gate_end{-1};

  std::array<held_note, max_held> m_held{};
  int m_held_count{};
  std::array<std::array<uint16_t, 128>, 16> m_sounding{};

  int64_t m_now{};
  int64_t m_last_end{};
  double m_last_quarter{-1e300};
  double m_rate{48000.};
  int m_frames{};
  bool m_started{};
  bool m_stop_requested{};
};
}
