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
#include <libremidi/message.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace mtk
{
namespace life
{
inline constexpr int max_width = 32;
inline constexpr int max_height = 16;

//! One bit per cell: bit x of rows[y] is column x of row y, row 0 the lowest
//! note.
using grid = std::array<uint32_t, max_height>;

//! Neighbour counts that make a cell be born or survive, bit n for n
//! neighbours.
struct rule
{
  uint16_t birth{1 << 3};
  uint16_t survive{(1 << 2) | (1 << 3)};
};

/**
 * @brief Reads a Life-like rule: "B3/S23", "b36/s23", "B3S23", "B2/S" or the
 * older survive/birth form "23/3". Never allocates.
 */
inline std::optional<rule> parse_rule(std::string_view s) noexcept
{
  rule r{0, 0};
  auto digits = [&](std::size_t& i, uint16_t& mask) {
    while(i < s.size() && s[i] >= '0' && s[i] <= '8')
      mask |= uint16_t(1u << (s[i++] - '0'));
  };
  auto skip = [&](std::size_t& i) {
    while(i < s.size() && (s[i] == ' ' || s[i] == '/' || s[i] == ',' || s[i] == '\t'))
      i++;
  };

  std::size_t i = 0;
  skip(i);
  if(i == s.size())
    return std::nullopt;

  if(s[i] >= '0' && s[i] <= '8')
  {
    // S/B: survive digits, a slash, birth digits
    digits(i, r.survive);
    if(i >= s.size() || s[i] != '/')
      return std::nullopt;
    i++;
    digits(i, r.birth);
    skip(i);
    return i == s.size() ? std::optional{r} : std::nullopt;
  }

  bool seen_b = false, seen_s = false;
  while(i < s.size())
  {
    const char c = s[i++];
    if((c == 'B' || c == 'b') && !seen_b)
    {
      seen_b = true;
      digits(i, r.birth);
    }
    else if((c == 'S' || c == 's') && !seen_s)
    {
      seen_s = true;
      digits(i, r.survive);
    }
    else
      return std::nullopt;
    skip(i);
  }
  if(!seen_b || !seen_s)
    return std::nullopt;
  return r;
}

struct pattern_cells
{
  grid rows{}; //!< row 0 is the first line of the text, the top one
  int width{};
  int height{};
};

/**
 * @brief Reads a drawn pattern: lines separated by '/', spaces or new lines,
 * '.', '0', '_' or '-' for a dead cell, any other character for a live one.
 * The first line is the top row. Lines beyond the grid's capacity are cut.
 */
inline pattern_cells parse_pattern(std::string_view s) noexcept
{
  pattern_cells p;
  int x = 0;
  bool in_line = false;
  for(char c : s)
  {
    if(c == '/' || c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == ',')
    {
      if(in_line)
      {
        p.height++;
        in_line = false;
        x = 0;
      }
      continue;
    }
    if(p.height >= max_height)
      break;
    in_line = true;
    const bool dead = c == '.' || c == '0' || c == '_' || c == '-';
    if(x < max_width)
    {
      if(!dead)
        p.rows[p.height] |= 1u << x;
      p.width = std::max(p.width, x + 1);
    }
    x++;
  }
  if(in_line)
    p.height++;
  p.height = std::min(p.height, max_height);
  return p;
}

//! Notes of a drum map or a melody, one per row: "36 38 42 46", "C2, D2, F#2".
struct note_map
{
  static constexpr int max_notes = max_height;
  std::array<uint8_t, max_notes> notes{};
  int count{};
  bool valid{true};

  void parse(std::string_view s) noexcept
  {
    count = 0;
    valid = true;
    std::size_t i = 0;
    while(i < s.size())
    {
      const char c = s[i];
      if(c == ' ' || c == ',' || c == '\t' || c == '\n' || c == '\r' || c == '/')
      {
        i++;
        continue;
      }
      int note = -1;
      if(c >= '0' && c <= '9')
      {
        int v = 0;
        while(i < s.size() && s[i] >= '0' && s[i] <= '9' && v <= 127)
          v = v * 10 + (s[i++] - '0');
        note = v;
      }
      else if(auto n = parse_note_name(s.substr(i)))
      {
        note = n->first;
        i += n->second;
      }
      if(note < 0 || note > 127 || count >= max_notes)
      {
        valid = false;
        count = 0;
        return;
      }
      notes[count++] = uint8_t(note);
    }
  }
};

//! Keeps a parsed value in sync with a text control, parsing again only when
//! the text changed; the last text is kept in a fixed buffer.
template <typename T>
struct cached_text
{
  static constexpr std::size_t max_text = 256;
  T parsed{};

  template <typename Parse>
  bool update(std::string_view text, Parse&& parse) noexcept
  {
    if(text.size() > max_text)
      text = text.substr(0, max_text);
    if(m_parsed && text == std::string_view{m_text.data(), m_size})
      return false;
    std::copy(text.begin(), text.end(), m_text.begin());
    m_size = text.size();
    m_parsed = true;
    parse(text, parsed);
    return true;
  }

private:
  std::array<char, max_text> m_text{};
  std::size_t m_size{};
  bool m_parsed{};
};

//! Scales of the rows, in semitones above the root, and their size.
struct scale_steps
{
  std::array<int8_t, 12> steps;
  int8_t count;
};

inline constexpr scale_steps scales[] = {
    {{0, 2, 4, 5, 7, 9, 11}, 7},  // Major
    {{0, 2, 3, 5, 7, 9, 10}, 7},  // Dorian
    {{0, 1, 3, 5, 7, 8, 10}, 7},  // Phrygian
    {{0, 2, 4, 6, 7, 9, 11}, 7},  // Lydian
    {{0, 2, 4, 5, 7, 9, 10}, 7},  // Mixolydian
    {{0, 2, 3, 5, 7, 8, 10}, 7},  // Minor
    {{0, 1, 3, 5, 6, 8, 10}, 7},  // Locrian
    {{0, 2, 3, 5, 7, 8, 11}, 7},  // Harmonic minor
    {{0, 2, 3, 5, 7, 9, 11}, 7},  // Melodic minor
    {{0, 2, 4, 7, 9}, 5},         // Major pentatonic
    {{0, 3, 5, 7, 10}, 5},        // Minor pentatonic
    {{0, 3, 5, 6, 7, 10}, 6},     // Blues
    {{0, 2, 4, 6, 8, 10}, 6},     // Whole tone
    {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 12}, // Chromatic
};
inline constexpr int scale_count = int(std::size(scales));

inline constexpr int scale_semitones(int scale, int degree) noexcept
{
  const auto& s = scales[std::clamp(scale, 0, scale_count - 1)];
  const int octave = degree / s.count;
  return octave * 12 + s.steps[degree % s.count];
}

//! The state the layer paints, sent from the processing thread at most once
//! per tick. Trivial, so the message bus copies it as bytes.
struct view_message
{
  grid rows;
  uint32_t generation;
  uint16_t population;
  uint8_t width;
  uint8_t height;
  int8_t column; //!< the play-head, -1 before the first step
  uint8_t running;
};

//! A click on a cell of the layer, in grid coordinates (row 0 at the bottom).
struct toggle_message
{
  uint8_t x;
  uint8_t y;
};

inline bool cell(const grid& g, int x, int y) noexcept
{
  return (g[y] >> x) & 1u;
}

/**
 * @brief The layer's view of the world: live cells, the cells born in the
 * current generation, the play-head column.
 *
 * Births are what is alive now and was not in the generation before, so the
 * view keeps the previous generation it saw; a generation 0 is all births.
 */
struct life_view
{
  static constexpr double width() { return 224.; }
  static constexpr double height() { return 112.; }

  view_message state{};
  grid previous{};
  bool has_state{};
  bool previous_valid{};

  std::function<void()> update;
  std::function<void(toggle_message)> on_toggle;

  void receive(const view_message& m) noexcept
  {
    if(!has_state || m.generation != state.generation)
    {
      previous_valid = has_state && m.generation == state.generation + 1;
      previous = state.rows;
    }
    state = m;
    has_state = true;
    if(update)
      update();
  }

  bool born(int x, int y) const noexcept
  {
    if(!cell(state.rows, x, y))
      return false;
    if(state.generation == 0)
      return true;
    return previous_valid && !cell(previous, x, y);
  }

  double cell_size() const noexcept
  {
    const int w = std::max<int>(1, state.width), h = std::max<int>(1, state.height);
    return std::floor(std::min(width() / w, height() / h));
  }

  void paint(auto ctx)
  {
    ctx.set_fill_color({24, 24, 26, 255});
    ctx.begin_path();
    ctx.draw_rect(0., 0., width(), height());
    ctx.fill();

    if(!has_state || state.width == 0 || state.height == 0)
    {
      ctx.set_fill_color({140, 140, 140, 255});
      ctx.begin_path();
      ctx.draw_text(10., height() / 2. - 4., "Play to see the world.");
      ctx.draw_text(10., height() / 2. + 12., "Click a cell to toggle it.");
      ctx.fill();
      return;
    }

    const int w = state.width, h = state.height;
    const double c = cell_size();
    const double gap = c >= 6. ? 1. : 0.;

    for(int y = 0; y < h; y++)
    {
      const double py = (h - 1 - y) * c;
      for(int x = 0; x < w; x++)
      {
        const bool alive = cell(state.rows, x, y);
        const bool head = x == state.column;
        if(!alive)
        {
          if(head)
            ctx.set_fill_color({74, 64, 34, 255});
          else
            ctx.set_fill_color({40, 40, 44, 255});
        }
        else if(born(x, y))
        {
          if(head)
            ctx.set_fill_color({255, 250, 210, 255});
          else
            ctx.set_fill_color({120, 220, 255, 255});
        }
        else
        {
          if(head)
            ctx.set_fill_color({255, 214, 90, 255});
          else
            ctx.set_fill_color({255, 176, 30, 255});
        }
        ctx.begin_path();
        ctx.draw_rect(x * c + gap, py + gap, c - gap, c - gap);
        ctx.fill();
      }
    }
  }

  bool mouse_press(double px, double py)
  {
    // Only the running world takes clicks: there is no other to send them to
    if(!has_state || !state.running || !on_toggle || state.width == 0
       || state.height == 0)
      return false;
    const double c = cell_size();
    if(c <= 0.)
      return false;
    const int x = int(px / c);
    const int row = int(py / c);
    if(px < 0. || py < 0. || x >= state.width || row >= state.height)
      return false;
    const int y = state.height - 1 - row;
    state.rows[y] ^= 1u << x;
    on_toggle({uint8_t(x), uint8_t(y)});
    return true;
  }
  void mouse_move(double, double) { }
  void mouse_release(double, double) { }
};
}

/**
 * @brief A Game of Life plays notes: a play-head sweeps the grid column by
 * column on the musical grid, and the live cells of its column play the notes
 * of their rows.
 *
 * Rows are scale degrees from the root (row 0, the bottom one), or the notes
 * of a note map for drums. The world evolves one generation per sweep of the
 * play-head, or every N steps, so a phrase repeats before it changes.
 * With Births, only the cells born in the current generation play.
 *
 * Velocity is the base velocity plus Crowd times (neighbours - 2): crowded
 * cells play louder, isolated ones softer, or the reverse with a negative
 * Crowd. When a column holds more live cells than Max notes, the ones played
 * are spread evenly over it.
 *
 * Auto reseed starts a new world when this one dies out, or after it has
 * repeated a recent generation (still lifes, oscillators of period up to 16)
 * for Reseed after generations. A random world comes from Seed (0: a new one
 * each time); a pattern comes back centred, then at seeded random places.
 *
 * Synced, steps land on the musical grid of the timeline; free, they are
 * Rate seconds apart from the start of playback. Stop and rewind end every
 * note and restart the world from its seed. Clicking a cell of the layer
 * toggles it in the running world. Incoming MIDI passes through; CC 120 and
 * 123 also end the notes that sound.
 */
struct MidiLife
{
  halp_meta(name, "Midi life")
  halp_meta(c_name, "midi_life")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(
      description, "A Game of Life sequencer: a play-head sweeps the evolving grid and "
                   "live cells play the notes of their rows")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-life.html")
  halp_meta(uuid, "04db97fd-2ccf-4271-a5a6-d66552baf0d7")

  enum Evolve
  {
    EachSweep,
    EveryNSteps
  };
  enum RuleChoice
  {
    RuleLife,
    RuleHighLife,
    RuleSeeds,
    RuleDayNight,
    RuleMaze,
    RuleMove,
    RuleCustom
  };
  enum Start
  {
    StartRandom,
    StartText,
    StartGlider,
    StartBlinker,
    StartToad,
    StartRPentomino,
    StartAcorn,
    StartDiehard,
    StartSpaceship,
    StartPentadecathlon,
    StartPulsar
  };
  enum Play
  {
    PlayLive,
    PlayBirths
  };

  static constexpr int max_steps_per_tick = 256;

  struct ins
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;

    struct : halp::time_chooser<"Rate", halp::range{0.01, 4., 0.125}>
    {
      halp_meta(
          description, "Time between two steps of the play-head; synced, a note value "
                       "on the bars' grid")
    } rate;
    struct : halp::hslider_i32<"Gate", halp::irange{5, 100, 50}>
    {
      halp_meta(description, "Length of a note, in percent of a step")
    } gate;
    struct : halp::hslider_i32<"Swing", halp::irange{0, 100, 0}>
    {
      halp_meta(
          description, "Delays every second step: 50 puts it on the triplet, 100 at "
                       "5/6 of the pair")
    } swing;
    struct
    {
      halp_meta(name, "Evolve");
      halp_meta(
          description, "When the world moves to its next generation: each time the "
                       "play-head starts a sweep, or every few steps");
      enum widget
      {
        combobox
      };
      struct range
      {
        halp::combo_pair<int> values[2]{{"Each sweep", EachSweep}, {"Every N steps", EveryNSteps}};
        int init{EachSweep};
      };
      int value{EachSweep};
    } evolve;
    struct : halp::spinbox_i32<"Steps/gen", halp::irange{1, 64, 4}>
    {
      halp_meta(description, "Steps of the play-head between two generations")
    } steps_per_gen;

    struct : halp::spinbox_i32<"Width", halp::irange{4, life::max_width, 16}>
    {
      halp_meta(description, "Columns of the grid: steps in a sweep")
    } width;
    struct : halp::spinbox_i32<"Height", halp::irange{2, life::max_height, 8}>
    {
      halp_meta(description, "Rows of the grid: one note each")
    } height;
    struct : halp::toggle<"Wrap", halp::toggle_setup{.init = true}>
    {
      halp_meta(description, "The edges touch the opposite ones, as on a torus")
    } wrap;
    struct
    {
      halp_meta(name, "Rule");
      halp_meta(
          description, "How cells are born and survive. Life B3/S23, HighLife B36/S23 "
                       "(replicators), Seeds B2/S (explosive), Day and Night "
                       "B3678/S34678, Maze B3/S12345 (fills up), Move B368/S245");
      enum widget
      {
        combobox
      };
      struct range
      {
        halp::combo_pair<int> values[7]{
            {"Life", RuleLife},         {"HighLife", RuleHighLife}, {"Seeds", RuleSeeds},
            {"Day and Night", RuleDayNight}, {"Maze", RuleMaze},     {"Move", RuleMove},
            {"Custom", RuleCustom}};
        int init{RuleLife};
      };
      int value{RuleLife};
    } rule;
    struct : halp::lineedit<"Custom rule", "B3/S23">
    {
      halp_meta(
          description, "A Life-like rule: B then the neighbour counts that give birth, "
                       "S then those that keep a cell alive, e.g. B36/S23")
    } custom_rule;

    struct
    {
      halp_meta(name, "Start");
      halp_meta(
          description, "The first world: a random fill, the drawn pattern, or a "
                       "classic pattern in the centre");
      enum widget
      {
        combobox
      };
      struct range
      {
        halp::combo_pair<int> values[11]{
            {"Random", StartRandom},
            {"Pattern", StartText},
            {"Glider", StartGlider},
            {"Blinker", StartBlinker},
            {"Toad", StartToad},
            {"R-pentomino", StartRPentomino},
            {"Acorn", StartAcorn},
            {"Diehard", StartDiehard},
            {"Spaceship", StartSpaceship},
            {"Pentadecathlon", StartPentadecathlon},
            {"Pulsar", StartPulsar}};
        int init{StartRandom};
      };
      int value{StartRandom};
    } start;
    struct : halp::lineedit<"Pattern", ".o./..o/ooo">
    {
      halp_meta(
          description, "Rows from the top, separated by '/' or spaces: '.' a dead "
                       "cell, 'o' or '#' a live one")
    } pattern;
    struct : halp::hslider_i32<"Density", halp::irange{1, 100, 30}>
    {
      halp_meta(description, "Percentage of live cells in a random world")
    } density;
    struct : halp::spinbox_i32<"Seed", halp::irange{0, 9999, 0}>
    {
      halp_meta(
          description, "The same seed gives the same worlds on every play; 0 gives "
                       "new ones each time")
    } seed;
    struct : halp::impulse_button<"Reseed">
    {
      halp_meta(description, "Start a new world at the next step")
    } reseed;
    struct : halp::toggle<"Auto reseed", halp::toggle_setup{.init = true}>
    {
      halp_meta(
          description, "Start a new world when this one dies out or keeps repeating")
    } auto_reseed;
    struct : halp::spinbox_i32<"Reseed after", halp::irange{1, 64, 4}>
    {
      halp_meta(
          description, "Generations a repeating world plays before a new one starts")
    } patience;

    struct
    {
      halp_meta(name, "Play");
      halp_meta(
          description, "Every live cell under the play-head plays, or only those born "
                       "in this generation");
      enum widget
      {
        combobox
      };
      struct range
      {
        halp::combo_pair<int> values[2]{{"Live cells", PlayLive}, {"Births", PlayBirths}};
        int init{PlayLive};
      };
      int value{PlayLive};
    } play;
    struct
    {
      halp_meta(name, "Root");
      halp_meta(description, "Note of the bottom row");
      enum widget
      {
        combobox
      };
      struct range
      {
        halp::combo_pair<int> values[12]{
            {"C", 0},  {"C#", 1}, {"D", 2},  {"D#", 3}, {"E", 4},   {"F", 5},
            {"F#", 6}, {"G", 7},  {"G#", 8}, {"A", 9},  {"A#", 10}, {"B", 11}};
        int init{0};
      };
      int value{0};
    } root;
    struct : halp::spinbox_i32<"Octave", halp::irange{-1, 8, 3}>
    {
      halp_meta(description, "Octave of the bottom row; C3 is MIDI note 48")
    } octave;
    struct
    {
      halp_meta(name, "Scale");
      halp_meta(description, "Scale the rows climb, one degree per row");
      enum widget
      {
        combobox
      };
      struct range
      {
        halp::combo_pair<int> values[life::scale_count]{
            {"Major", 0},          {"Dorian", 1},        {"Phrygian", 2},
            {"Lydian", 3},         {"Mixolydian", 4},    {"Minor", 5},
            {"Locrian", 6},        {"Harmonic minor", 7}, {"Melodic minor", 8},
            {"Major penta", 9},    {"Minor penta", 10},  {"Blues", 11},
            {"Whole tone", 12},    {"Chromatic", 13}};
        int init{9};
      };
      int value{9};
    } mode;
    struct : halp::lineedit<"Note map", "">
    {
      halp_meta(
          description, "Notes of the rows from the bottom, e.g. 36 38 42 46 for drums; "
                       "rows past the list are silent. Empty: the scale.")
    } note_map;
    struct : halp::spinbox_i32<"Channel", halp::irange{1, 16, 1}>
    {
      halp_meta(description, "Channel of the notes")
    } channel;
    struct : halp::hslider_i32<"Velocity", halp::irange{1, 127, 90}>
    {
      halp_meta(description, "Velocity of a cell with two neighbours")
    } velocity;
    struct : halp::hslider_i32<"Crowd", halp::irange{-30, 30, 10}>
    {
      halp_meta(
          description, "Velocity added per neighbour above two, removed per neighbour "
                       "below: crowded cells louder, or softer when negative")
    } crowd;
    struct : halp::spinbox_i32<"Max notes", halp::irange{1, life::max_height, 4}>
    {
      halp_meta(description, "Most notes a step plays, spread over its live cells")
    } max_notes;
  } inputs;

  struct
  {
    halp::midi_bus<"MIDI", libremidi::message> midi;
    struct : halp::val_port<"Population", int>
    {
      halp_meta(description, "Live cells in the grid")
    } population;
    struct : halp::val_port<"Generation", int>
    {
      halp_meta(description, "Generations since the world started")
    } generation;
  } outputs;

  std::function<void(life::view_message)> send_message;

  void process_message(const life::toggle_message& m) noexcept
  {
    if(m.x >= life::max_width || m.y >= life::max_height)
      return;
    m_cells[m.y] ^= 1u << m.x;
    m_population = count_cells(m_cells);
    m_view_dirty = true;
  }

  struct ui
  {
    halp_meta(name, "Midi life")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Time")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::rate> rate;
      halp::item<&ins::gate> gate;
      halp::item<&ins::swing> swing;
      halp::item<&ins::evolve> evolve;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::evolve, EveryNSteps>{};
        }
        halp::item<&ins::steps_per_gen> steps_per_gen;
      } steps_per_gen;
    } time;

    struct
    {
      halp_meta(name, "World")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      // Not named width and height: the layout reads those as its size
      halp::item<&ins::width> columns;
      halp::item<&ins::height> rows;
      halp::item<&ins::wrap> wrap;
      halp::item<&ins::rule> rule;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::rule, RuleCustom>{};
        }
        halp::item<&ins::custom_rule> custom_rule;
      } custom_rule;
    } world;

    struct
    {
      halp_meta(name, "Seed")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::start> start;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::start, StartText>{};
        }
        halp::item<&ins::pattern> pattern;
      } pattern;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::start, StartRandom>{};
        }
        halp::item<&ins::density> density;
      } density;
      halp::item<&ins::seed> seed;
      halp::item<&ins::reseed> reseed;
      halp::item<&ins::auto_reseed> auto_reseed;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        static constexpr auto condition()
        {
          return halp::enabled_when<&ins::auto_reseed, true>{};
        }
        halp::item<&ins::patience> patience;
      } patience;
    } seeding;

    struct
    {
      halp_meta(name, "Notes")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::play> play;
      halp::item<&ins::root> root;
      halp::item<&ins::octave> octave;
      halp::item<&ins::mode> mode;
      halp::item<&ins::note_map> note_map;
      halp::item<&ins::channel> channel;
    } notes;

    struct
    {
      halp_meta(name, "Dynamics")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::velocity> velocity;
      halp::item<&ins::crowd> crowd;
      halp::item<&ins::max_notes> max_notes;
    } dynamics;

    struct
    {
      halp_meta(name, "Grid")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::custom_actions_item<life::life_view> view;
    } grid;

    struct bus
    {
      std::function<void(life::toggle_message)> send_message;

      void init(ui& self)
      {
        self.grid.view.on_toggle = [this](life::toggle_message m) {
          if(send_message)
            send_message(m);
        };
      }

      static void process_message(ui& self, life::view_message msg)
      {
        self.grid.view.receive(msg);
      }
    };
  };

  void prepare(halp::setup s) noexcept
  {
    if(s.rate > 0.)
      m_rate = s.rate;
  }

  void stop() noexcept { m_stop_requested = true; }

  MidiLife() noexcept
  {
    m_free_rng = uint64_t(std::chrono::steady_clock::now().time_since_epoch().count())
                 ^ uint64_t(reinterpret_cast<uintptr_t>(this));
  }

  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& t) noexcept
  {
    m_frames = t.frames > 0 ? t.frames : 0;
    cache_parameters();

    if(m_stop_requested)
    {
      m_stop_requested = false;
      end_all(0, 0);
      m_queue.clear();
      restart();
      m_view_dirty = true;
      m_running = false;
      m_has_last = false;
    }
    else if(m_has_last && m_frames > 0 && t.position_in_frames < m_last_end)
    {
      // Rewind or loop: the take starts over
      end_all(0, 0);
      m_queue.clear();
      restart();
    }

    if(!m_ready)
      restart();

    if(inputs.reseed.value)
      m_reseed_requested = true;

    if(m_frames > 0)
    {
      if(!m_has_last)
        m_free_next = m_now;
      m_has_last = true;
      m_running = true;
      m_last_end = t.position_in_frames + m_frames;
      run_steps(t);
    }

    for(const auto& m : inputs.midi)
    {
      const int64_t ts
          = m_frames > 0 ? std::clamp<int64_t>(m.timestamp, 0, m_frames - 1) : 0;
      m_queue.release(m_now, int(ts), [this](libremidi::message&& e) { send(std::move(e)); });
      handle(m, ts);
    }
    m_queue.release(m_now, m_frames, [this](libremidi::message&& e) { send(std::move(e)); });

    outputs.population.value = m_population;
    outputs.generation.value = int(m_generation);

    if(send_message && (m_view_dirty || ++m_view_idle >= 16))
    {
      m_view_dirty = false;
      m_view_idle = 0;
      send_message(view());
    }
    m_now += m_frames;
  }

  // Observers, for tests and the layer
  const life::grid& cells() const noexcept { return m_cells; }
  uint32_t generation() const noexcept { return m_generation; }
  int population() const noexcept { return m_population; }
  int column() const noexcept { return m_column; }
  int reseeds() const noexcept { return m_reseeds; }
  int sounding() const noexcept
  {
    int n = 0;
    for(auto& c : m_sounding)
      for(auto v : c)
        n += v;
    return n;
  }

  life::view_message view() const noexcept
  {
    life::view_message m;
    m.rows = m_cells;
    m.generation = m_generation;
    m.population = uint16_t(m_population);
    m.width = uint8_t(m_w);
    m.height = uint8_t(m_h);
    m.column = int8_t(m_column);
    m.running = m_running;
    return m;
  }

  //! One generation of `g` on a `w` x `h` grid.
  static life::grid
  next_generation(const life::grid& g, int w, int h, bool wrap, life::rule r) noexcept
  {
    life::grid out{};
    for(int y = 0; y < h; y++)
      for(int x = 0; x < w; x++)
      {
        const int n = neighbours(g, x, y, w, h, wrap);
        const bool alive = life::cell(g, x, y);
        if(alive ? (r.survive >> n) & 1 : (r.birth >> n) & 1)
          out[y] |= 1u << x;
      }
    return out;
  }

  static int
  neighbours(const life::grid& g, int x, int y, int w, int h, bool wrap) noexcept
  {
    int n = 0;
    for(int dy = -1; dy <= 1; dy++)
      for(int dx = -1; dx <= 1; dx++)
      {
        if(dx == 0 && dy == 0)
          continue;
        int nx = x + dx, ny = y + dy;
        if(wrap)
        {
          nx = (nx + w) % w;
          ny = (ny + h) % h;
        }
        else if(nx < 0 || ny < 0 || nx >= w || ny >= h)
          continue;
        n += life::cell(g, nx, ny);
      }
    return n;
  }

private:
  // ------------------------------------------------------------- parameters

  void cache_parameters() noexcept
  {
    m_w = std::clamp(inputs.width.value, 4, life::max_width);
    m_h = std::clamp(inputs.height.value, 2, life::max_height);
    m_wrap = inputs.wrap.value;

    m_custom_rule.update(inputs.custom_rule.value, [](std::string_view s, auto& r) {
      r = life::parse_rule(s);
    });
    switch(inputs.rule.value)
    {
      default:
      case RuleLife:
        m_rule = {1 << 3, (1 << 2) | (1 << 3)};
        break;
      case RuleHighLife:
        m_rule = {(1 << 3) | (1 << 6), (1 << 2) | (1 << 3)};
        break;
      case RuleSeeds:
        m_rule = {1 << 2, 0};
        break;
      case RuleDayNight:
        m_rule = {(1 << 3) | (1 << 6) | (1 << 7) | (1 << 8),
                  (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 8)};
        break;
      case RuleMaze:
        m_rule = {1 << 3, (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5)};
        break;
      case RuleMove:
        m_rule = {(1 << 3) | (1 << 6) | (1 << 8), (1 << 2) | (1 << 4) | (1 << 5)};
        break;
      case RuleCustom:
        m_rule = m_custom_rule.parsed ? *m_custom_rule.parsed : life::rule{};
        break;
    }

    m_note_map.update(inputs.note_map.value, [](std::string_view s, auto& m) {
      m.parse(s);
    });
    m_pattern.update(inputs.pattern.value, [](std::string_view s, auto& p) {
      p = life::parse_pattern(s);
    });
  }

  // ------------------------------------------------------------------ steps

  //! The step length as a note value, snapped to the note values a time
  //! chooser offers so that float rounding does not move the grid.
  double synced_rate(const halp::tick_musical& t) const noexcept
  {
    const double tempo = t.tempo > 0. ? t.tempo : 120.;
    const double whole = double(inputs.rate.value) * tempo / 240.;
    if(!(whole > 0.))
      return 0.;
    const double ticks = std::round(whole * 6720.);
    const double snapped = ticks > 0. && std::abs(ticks - whole * 6720.) < 0.01 * ticks
                               ? ticks / 6720.
                               : whole;
    return 1. / snapped;
  }

  void run_steps(const halp::tick_musical& t) noexcept
  {
    const double dq = t.end_position_in_quarters - t.start_position_in_quarters;
    const bool synced = inputs.rate.sync && dq > 0.;
    if(synced)
    {
      const double rate = synced_rate(t);
      if(!(rate > 0.))
        return;
      const double unit_q = rate <= 1. ? quarters_in_bar(t) / rate : 4. / rate;
      m_step_frames = unit_q * m_frames / dq;
      for_each_grid_point(t, rate, [&](int frame) { step(m_now + frame); });
    }
    else
    {
      if(dq < 0.)
        return;
      const double len = std::max(1., double(inputs.rate.value) * m_rate);
      m_step_frames = len;
      const int64_t end = m_now + m_frames;
      int guard = 0;
      while(m_free_next < end && guard++ < max_steps_per_tick)
      {
        step(std::max(m_free_next, m_now));
        m_free_next += int64_t(std::llround(len));
      }
    }
  }

  static double quarters_in_bar(const halp::tick_musical& t) noexcept
  {
    return (t.signature.num > 0 && t.signature.denom > 0)
               ? 4. * t.signature.num / t.signature.denom
               : 4.;
  }

  //! The points of halp::tick_musical::get_quantification_date for forward
  //! playback, without allocating: subdivisions of the quarter restart at
  //! each bar line, a bar or longer counts from the last signature change.
  template <typename F>
  static void
  for_each_grid_point(const halp::tick_musical& t, double rate, F&& f) noexcept
  {
    const double start = t.start_position_in_quarters;
    const double end = t.end_position_in_quarters;
    if(!(end > start) || t.frames <= 0 || !(rate > 0.))
      return;
    constexpr double eps = 1e-9;
    int count = 0;
    auto push = [&](double p) {
      if(p >= end || count >= max_steps_per_tick)
        return false;
      const int64_t fr = std::clamp(
          halp::musical_to_frame(p, start, end, t.frames), int64_t(0),
          int64_t(t.frames - 1));
      f(int(fr));
      count++;
      return true;
    };

    if(rate <= 1.)
    {
      const double unit = quarters_in_bar(t) / rate;
      const double origin = t.last_signature_change;
      const int64_t first = int64_t(std::ceil((start - origin) / unit - eps));
      for(int64_t k = first; k * unit + origin < end - eps; k++)
        if(!push(k * unit + origin))
          break;
      return;
    }

    const double unit = 4. / rate;
    bool go = true;
    t.for_each_bar_segment(start, end, [&](double bar_line, double next_bar) {
      if(!go)
        return;
      for(int k = 0;; k++)
      {
        const double p = bar_line + k * unit;
        if(p >= next_bar - eps || p > end + eps)
          return;
        if(p < start)
          continue;
        if(!push(p))
        {
          go = false;
          return;
        }
      }
    });
  }

  //! A step of the play-head at the absolute frame `at`.
  void step(int64_t at) noexcept
  {
    const int w = m_w;
    const int64_t s = m_step++;
    const int col = int(s % w);

    if(m_reseed_requested)
    {
      m_reseed_requested = false;
      reseed();
    }
    else if(s > 0)
    {
      const bool due = inputs.evolve.value == EachSweep
                           ? col == 0
                           : (s % std::max(1, inputs.steps_per_gen.value)) == 0;
      if(due)
        evolve();
    }
    m_column = col;
    m_view_dirty = true;

    // The cells that play, bottom to top
    int ys[life::max_height];
    int n = 0;
    const bool births = inputs.play.value == PlayBirths;
    for(int y = 0; y < m_h; y++)
    {
      if(!life::cell(m_cells, col, y))
        continue;
      if(births && m_generation > 0 && life::cell(m_previous, col, y))
        continue;
      ys[n++] = y;
    }
    if(n == 0)
      return;

    const int max = std::clamp(inputs.max_notes.value, 1, life::max_height);
    const int played = std::min(n, max);

    const int64_t delay = std::llround(
        swing_delay(s, inputs.swing.value / 100.) * m_step_frames);
    const int64_t on_at = at + std::max<int64_t>(0, delay);
    const int64_t len = std::max<int64_t>(
        1, std::llround(m_step_frames * std::clamp(inputs.gate.value, 1, 100) / 100.));
    const int ch = std::clamp(inputs.channel.value, 1, 16);

    for(int k = 0; k < played; k++)
    {
      // Spread evenly: the middle cell of each of `played` equal slices
      const int idx = played == n ? k : int((2 * k + 1) * n / (2 * played));
      const int y = ys[idx];
      const int pitch = pitch_of(y);
      if(pitch < 0)
        continue;
      const int nb = neighbours(m_cells, col, y, w, m_h, m_wrap);
      const int vel = std::clamp(
          inputs.velocity.value + inputs.crowd.value * (nb - 2), 1, 127);
      m_queue.schedule(on_at, libremidi::channel_events::note_on(ch, pitch, vel));
      m_queue.schedule(on_at + len, libremidi::channel_events::note_off(ch, pitch, 0));
    }
  }

  //! The note of a row, or -1 when the row is silent.
  int pitch_of(int y) const noexcept
  {
    const auto& map = m_note_map.parsed;
    if(map.count > 0)
      return y < map.count ? map.notes[y] : -1;
    const int lowest = (std::clamp(inputs.octave.value, -1, 9) + 1) * 12
                       + std::clamp(inputs.root.value, 0, 11);
    const int p = lowest + life::scale_semitones(inputs.mode.value, y);
    return p >= 0 && p <= 127 ? p : -1;
  }

  // ------------------------------------------------------------------ world

  void restart() noexcept
  {
    m_ready = true;
    m_step = 0;
    m_column = -1;
    m_reseeds = 0;
    m_reseed_requested = false;
    m_free_next = m_now;
    m_rng = 0;
    seed_world(true);
  }

  void reseed() noexcept
  {
    m_reseeds++;
    seed_world(false);
  }

  uint64_t next_random() noexcept
  {
    // splitmix64
    uint64_t& s = inputs.seed.value != 0 ? m_rng : m_free_rng;
    uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }

  void seed_world(bool first) noexcept
  {
    if(inputs.seed.value != 0)
      m_rng = uint64_t(inputs.seed.value) * 0x2545F4914F6CDD1Dull
              + uint64_t(m_reseeds) * 0x9E3779B97F4A7C15ull;

    m_cells = {};
    const int w = m_w, h = m_h;
    if(inputs.start.value == StartRandom)
    {
      const uint64_t threshold
          = uint64_t(std::clamp(inputs.density.value, 0, 100)) * (UINT64_MAX / 100);
      for(int y = 0; y < h; y++)
        for(int x = 0; x < w; x++)
          if(next_random() < threshold)
            m_cells[y] |= 1u << x;
    }
    else
    {
      const life::pattern_cells p = inputs.start.value == StartText
                                        ? m_pattern.parsed
                                        : life::parse_pattern(builtin_pattern());
      int x0 = (w - p.width) / 2;
      int top = (h - p.height) / 2;
      if(!first)
      {
        // A pattern comes back somewhere else, so the tune is new
        x0 = int(next_random() % uint64_t(w));
        top = int(next_random() % uint64_t(h));
      }
      for(int r = 0; r < p.height; r++)
        for(int c = 0; c < p.width; c++)
        {
          if(!((p.rows[r] >> c) & 1u))
            continue;
          int x = x0 + c, row = top + r;
          if(m_wrap || !first)
          {
            x = ((x % w) + w) % w;
            row = ((row % h) + h) % h;
          }
          if(x < 0 || x >= w || row < 0 || row >= h)
            continue;
          const int y = h - 1 - row;
          m_cells[y] |= 1u << x;
        }
    }

    m_previous = {};
    m_generation = 0;
    m_stale = 0;
    m_history_count = 0;
    m_history_pos = 0;
    m_population = count_cells(m_cells);
    remember(hash(m_cells));
    m_view_dirty = true;
  }

  std::string_view builtin_pattern() const noexcept
  {
    switch(inputs.start.value)
    {
      case StartGlider:
        return ".o./..o/ooo";
      case StartBlinker:
        return "ooo";
      case StartToad:
        return ".ooo/ooo.";
      case StartRPentomino:
        return ".oo/oo./.o.";
      case StartAcorn:
        return ".o...../...o.../oo..ooo";
      case StartDiehard:
        return "......o./oo....../.o...ooo";
      case StartSpaceship:
        return ".o..o/o..../o...o/oooo.";
      case StartPentadecathlon:
        return "..o....o../oo.oooo.oo/..o....o..";
      case StartPulsar:
        return "..ooo...ooo../............./o....o.o....o/o....o.o....o/"
               "o....o.o....o/..ooo...ooo../............./..ooo...ooo../"
               "o....o.o....o/o....o.o....o/o....o.o....o/............./"
               "..ooo...ooo..";
      default:
        return "";
    }
  }

  void evolve() noexcept
  {
    const life::grid next = next_generation(m_cells, m_w, m_h, m_wrap, m_rule);
    const int pop = count_cells(next);
    const uint64_t hsh = hash(next);
    const bool repeating = seen(hsh);

    m_stale = repeating ? m_stale + 1 : 0;
    if(inputs.auto_reseed.value
       && (pop == 0 || m_stale > std::max(1, inputs.patience.value)))
    {
      reseed();
      return;
    }

    m_previous = m_cells;
    m_cells = next;
    m_population = pop;
    m_generation++;
    remember(hsh);
  }

  int count_cells(const life::grid& g) const noexcept
  {
    const uint32_t mask = m_w >= 32 ? 0xFFFFFFFFu : ((1u << m_w) - 1u);
    int n = 0;
    for(int y = 0; y < m_h; y++)
      n += std::popcount(g[y] & mask);
    return n;
  }

  uint64_t hash(const life::grid& g) const noexcept
  {
    uint64_t h = 1469598103934665603ull ^ uint64_t(m_w) ^ (uint64_t(m_h) << 8);
    for(int y = 0; y < m_h; y++)
    {
      h ^= g[y];
      h *= 1099511628211ull;
    }
    return h;
  }

  bool seen(uint64_t h) const noexcept
  {
    for(int i = 0; i < m_history_count; i++)
      if(m_history[i] == h)
        return true;
    return false;
  }

  void remember(uint64_t h) noexcept
  {
    m_history[m_history_pos] = h;
    m_history_pos = (m_history_pos + 1) % int(m_history.size());
    m_history_count = std::min(m_history_count + 1, int(m_history.size()));
  }

  // ----------------------------------------------------------------- output

  //! Per channel and pitch, how many notes sound: a note-on over a sounding
  //! pitch re-attacks it, and the pitch ends with the last of its notes.
  void send(libremidi::message&& m) noexcept
  {
    const int ts = int(m.timestamp);
    const int c = channel_of(m);
    const int p = m.bytes[1] & 127;
    auto& n = m_sounding[(c - 1) & 15][p];
    if(is_note_on(m))
    {
      if(n > 0)
        push(libremidi::channel_events::note_off(c, p, 0), ts);
      if(n < 0xFF)
        n++;
      push(std::move(m), ts);
    }
    else if(is_note_off(m))
    {
      if(n == 0)
        return;
      if(--n == 0)
        push(std::move(m), ts);
    }
    else
      push(std::move(m), ts);
  }

  void push(libremidi::message m, int64_t ts) noexcept
  {
    m.timestamp = ts;
    outputs.midi.push_back(std::move(m));
  }

  void handle(const libremidi::message& m, int64_t ts) noexcept
  {
    if(is_all_notes_off(m))
      end_all(channel_of(m), ts);
    push(m, ts);
  }

  //! Ends what sounds on one channel or (0) all. Queued note-offs of those
  //! notes find nothing sounding and are dropped.
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
  }

  life::grid m_cells{};
  life::grid m_previous{};
  life::rule m_rule{};
  life::cached_text<std::optional<life::rule>> m_custom_rule;
  life::cached_text<life::note_map> m_note_map;
  life::cached_text<life::pattern_cells> m_pattern;
  std::array<uint64_t, 16> m_history{};
  int m_history_count{};
  int m_history_pos{};
  int m_stale{};

  std::array<std::array<uint8_t, 128>, 16> m_sounding{};
  event_queue<2048> m_queue;

  double m_rate{48000.};
  double m_step_frames{6000.};
  int64_t m_now{};
  int64_t m_free_next{};
  int64_t m_last_end{};
  int64_t m_step{};
  uint64_t m_rng{};
  uint64_t m_free_rng{};
  uint32_t m_generation{};
  int m_population{};
  int m_reseeds{};
  int m_column{-1};
  int m_w{16};
  int m_h{8};
  int m_frames{};
  int m_view_idle{};
  bool m_wrap{true};
  bool m_ready{};
  bool m_has_last{};
  bool m_running{};
  bool m_view_dirty{true};
  bool m_stop_requested{};
  bool m_reseed_requested{};
};
}
