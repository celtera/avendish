#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Advanced/MidiScaler/MidiCommon/ProgressionTable.hpp>
#include <Advanced/MidiScaler/MidiCommon/Theory.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string_view>

namespace mtk
{
/**
 * @brief Roman-numeral chord symbols, in the notation of free-midi-chords.
 *
 * free-midi-chords (https://github.com/ldrolez/free-midi-chords, MIT) writes
 * its progressions as roman numerals and renders them with chords2midi on top
 * of a fork of python-mingus (https://github.com/ldrolez/python-mingus). The
 * meaning of a symbol is the one mingus gives it; every rule below names the
 * function it comes from.
 *
 * Grammar of a symbol:
 *
 *   [b|#]? NUMERAL SUFFIX? ('/' [b|#]? NUMERAL SUFFIX?)?
 *
 * - NUMERAL is I to VII in any case (progressions.parse_string upper-cases
 *   every I and V it reads): `ii`, `II` and `Ii` are the same degree. The case
 *   is only a reminder of the chord's quality and is kept for display.
 * - No suffix: the diatonic triad of that degree in the key
 *   (progressions.to_chords calls chords.I(key) ... chords.VII(key), i.e.
 *   chords.triads(key)[degree], thirds stacked on the key's own notes).
 *   `iii` in C major is E minor, `III` too.
 * - Suffix `7`: the diatonic seventh chord (to_chords calls chords.I7(key)
 *   ... chords.VII7(key), chords.sevenths(key)). `V7` in C major is G7, `I7`
 *   is Cmaj7, `ii7` is Dm7, `vii7` is Bm7b5. mingus' docstring: "I7 will get
 *   you a major seventh chord. If you specifically want a dominant seventh,
 *   use Idom7."
 * - Any other suffix: the chord of chords.chord_shorthand[suffix], built on
 *   the root of the diatonic triad (to_chords: `r = chords.I(key);
 *   r = chord_shorthand[suffix](r[0])`). The suffix decides the quality, the
 *   case of the numeral does not: `IVm` and `ivm` are both F minor in C.
 *   `M` and `m` force a major or minor triad, `dim` a diminished triad, `M-5`
 *   a major triad with a flat fifth (major_diminished_triad), `5` root and
 *   fifth, `6`/`M6` a major sixth chord, `m6`, `69` (sixth_ninth), `9` a
 *   dominant ninth, `M9`, `m9`, `add9` (major triad + 9), `madd9`,
 *   `sus2`, `sus4`, `M7`, `m7`, `dom7` (dominant_seventh), and the other
 *   entries of chord_shorthand listed in mingus_suffixes below.
 * - A flat or a sharp before the numeral lowers or raises every note of the
 *   chord by a semitone, after it is built (to_chords: notes.diminish /
 *   notes.augment on each note). The quality is the natural degree's: in C,
 *   `bIII` is the E minor triad lowered, i.e. Eb minor, which is why
 *   free-midi-chords writes `bIIIM` for Eb major. `bi` is B major: I lowered.
 * - `/` and a second symbol: a slash chord. chords2midi (c2m.py) splits the
 *   symbol at the first `/`, and the bass is the root of the second symbol's
 *   chord (`bass_root = to_chords(bass_part, key)[0][0]`); its suffix is
 *   checked but plays no part. `I/bVII` in C is C major over Bb.
 * - `X` (or `x`) is a rest. In a progression, symbols are separated by one
 *   space and every further space adds a rest (gen.py turns two spaces into
 *   ` X `). Anything from ` =` on (the moods in chords.py) is ignored.
 *
 * Departures from mingus, all on input it would reject or misread: a single
 * accidental only (mingus takes any number); an unknown suffix is an error
 * (mingus raises KeyError, chords2midi then plays a rest); the suffixes that
 * contain a `/` (`m/M7`, `6/7`, `6/9`) are not accepted since chords2midi
 * splits there too (`mM7`, `67`, `69` are); and after mingus' own suffixes,
 * the chord type names of Theory.hpp (`maj7`, `m7-5`, `7sus4`, ...) and `2`
 * (free-midi-chords: same as add9) are accepted as well.
 *
 * Keys: free-midi-chords renders its major and modal progressions in a major
 * key and its minor ones in the relative natural minor (gen.py passes
 * `root_min.lower()`, and mingus' keys.get_notes of a lower-case key gives the
 * natural minor). Here the key is any root and any mode of Theory.hpp; the
 * diatonic chords follow that mode.
 */

//! The chord types of mingus' chords.chord_shorthand, by their suffix, as
//! semitones from the root (compound intervals for the 9th, 11th and 13th).
//! Where mingus' name differs from what the intervals suggest, mingus wins:
//! `M7+5` and `m7+` are augmented_minor_seventh (0 4 8 10), `7+` and `M7+`
//! augmented_major_seventh (0 4 8 11), `11` is eleventh (root, 5, b7, 11).
inline constexpr chord_type mingus_suffixes[] = {
    {"m", {0, 3, 7}, 3},              // minor_triad
    {"M", {0, 4, 7}, 3},              // major_triad
    {"dim", {0, 3, 6}, 3},            // diminished_triad
    {"aug", {0, 4, 8}, 3},            // augmented_triad
    {"+", {0, 4, 8}, 3},              // augmented_triad
    {"M-5", {0, 4, 6}, 3},            // major_diminished_triad
    {"7#5", {0, 4, 8, 10}, 4},        // augmented_minor_seventh
    {"M7+5", {0, 4, 8, 10}, 4},       // augmented_minor_seventh
    {"M7+", {0, 4, 8, 11}, 4},        // augmented_major_seventh
    {"m7+", {0, 4, 8, 10}, 4},        // augmented_minor_seventh
    {"7+", {0, 4, 8, 11}, 4},         // augmented_major_seventh
    {"sus47", {0, 5, 7, 10}, 4},      // suspended_seventh
    {"sus4", {0, 5, 7}, 3},           // suspended_fourth_triad
    {"sus2", {0, 2, 7}, 3},           // suspended_second_triad
    {"sus", {0, 5, 7}, 3},            // suspended_triad
    {"11", {0, 7, 10, 17}, 4},        // eleventh
    {"sus4b9", {0, 5, 7, 13}, 4},     // suspended_fourth_ninth
    {"susb9", {0, 5, 7, 13}, 4},      // suspended_fourth_ninth
    {"m7", {0, 3, 7, 10}, 4},         // minor_seventh
    {"M7", {0, 4, 7, 11}, 4},         // major_seventh
    {"dom7", {0, 4, 7, 10}, 4},       // dominant_seventh
    {"m7b5", {0, 3, 6, 10}, 4},       // minor_seventh_flat_five
    {"dim7", {0, 3, 6, 9}, 4},        // diminished_seventh
    {"mM7", {0, 3, 7, 11}, 4},        // minor_major_seventh
    {"m6", {0, 3, 7, 9}, 4},          // minor_sixth
    {"M6", {0, 4, 7, 9}, 4},          // major_sixth
    {"6", {0, 4, 7, 9}, 4},           // major_sixth
    {"67", {0, 4, 7, 9, 10}, 5},      // dominant_sixth
    {"69", {0, 4, 7, 9, 14}, 5},      // sixth_ninth
    {"9", {0, 4, 7, 10, 14}, 5},      // dominant_ninth
    {"7b9", {0, 4, 7, 10, 13}, 5},    // dominant_flat_ninth
    {"7#9", {0, 4, 7, 10, 15}, 5},    // dominant_sharp_ninth
    {"M9", {0, 4, 7, 11, 14}, 5},     // major_ninth
    {"m9", {0, 3, 7, 10, 14}, 5},     // minor_ninth
    {"add9", {0, 4, 7, 14}, 4},       // major_add_ninth
    {"madd9", {0, 3, 7, 14}, 4},      // minor_add_ninth
    {"7#11", {0, 4, 7, 10, 18}, 5},   // lydian_dominant_seventh
    {"m11", {0, 3, 7, 10, 17}, 5},    // minor_eleventh
    {"M13", {0, 4, 7, 11, 14, 21}, 6}, // major_thirteenth
    {"m13", {0, 3, 7, 10, 14, 21}, 6}, // minor_thirteenth
    {"13", {0, 4, 7, 10, 14, 21}, 6}, // dominant_thirteenth
    {"7b5", {0, 4, 6, 10}, 4},        // dominant_flat_five
    {"hendrix", {0, 4, 7, 10, 15}, 5}, // hendrix_chord
    {"7b12", {0, 4, 7, 10, 15}, 5},   // hendrix_chord
    {"5", {0, 7}, 2},                 // power chord
};

//! The chord type a suffix names: mingus' first, then Theory.hpp's, then
//! free-midi-chords' `2`. nullptr if none.
inline constexpr const chord_type* find_suffix(std::string_view s) noexcept
{
  for(const auto& c : mingus_suffixes)
    if(c.name == s)
      return &c;
  if(auto t = find_chord_type(s))
    return t;
  if(s == "2")
    return find_chord_type("add9");
  return nullptr;
}

//! One symbol of a progression. Positions refer to the parsed text.
struct chord_symbol
{
  bool rest{};
  bool lower{};        //!< numeral written in lower case
  bool seventh{};      //!< suffix `7`: the diatonic seventh chord
  bool slash{};
  int8_t degree{};     //!< 0 - 6
  int8_t accidental{}; //!< -1, 0, +1
  int8_t bass_degree{};
  int8_t bass_accidental{};
  const chord_type* type{}; //!< nullptr: diatonic triad or seventh
  uint16_t position{};
  uint16_t length{};

  std::string_view text(std::string_view source) const noexcept
  {
    if(position >= source.size())
      return {};
    return source.substr(position, length);
  }
};

enum class parse_status : uint8_t
{
  ok,
  empty,
  too_many,
  double_accidental,
  missing_numeral,
  bad_numeral,
  unknown_suffix,
  bad_bass
};

inline constexpr std::string_view parse_status_text(parse_status s) noexcept
{
  switch(s)
  {
    case parse_status::ok:
      return "OK";
    case parse_status::empty:
      return "No chord";
    case parse_status::too_many:
      return "Too many chords";
    case parse_status::double_accidental:
      return "One flat or sharp at most";
    case parse_status::missing_numeral:
      return "Expected a roman numeral";
    case parse_status::bad_numeral:
      return "Not a degree from I to VII";
    case parse_status::unknown_suffix:
      return "Unknown chord type";
    case parse_status::bad_bass:
      return "Bad bass after /";
  }
  return {};
}

//! Status, and the offending token's position and length in the text.
struct parse_error
{
  parse_status status{parse_status::ok};
  uint16_t position{};
  uint16_t length{};

  explicit operator bool() const noexcept { return status != parse_status::ok; }
};

struct parsed_progression
{
  static constexpr int max_chords = 64;
  std::array<chord_symbol, max_chords> chords{};
  int count{};

  std::span<const chord_symbol> symbols() const noexcept
  {
    return {chords.data(), std::size_t(count)};
  }
};

namespace detail
{
inline constexpr int numeral_index(std::string_view upper) noexcept
{
  constexpr std::string_view numerals[7]{"I", "II", "III", "IV", "V", "VI", "VII"};
  for(int i = 0; i < 7; i++)
    if(numerals[i] == upper)
      return i;
  return -1;
}

//! [b|#]? NUMERAL: reads it from s at i. Returns a status; i is moved past it.
inline constexpr parse_status
read_degree(std::string_view s, std::size_t& i, int8_t& degree, int8_t& acc, bool& lower)
{
  acc = 0;
  if(i < s.size() && (s[i] == 'b' || s[i] == '#'))
  {
    acc = s[i] == 'b' ? -1 : 1;
    i++;
    if(i < s.size() && (s[i] == 'b' || s[i] == '#'))
      return parse_status::double_accidental;
  }
  char buf[8]{};
  std::size_t n = 0;
  const std::size_t start = i;
  while(i < s.size())
  {
    const char c = s[i];
    const char u = (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c;
    if(u != 'I' && u != 'V')
      break;
    if(n < sizeof(buf))
      buf[n] = u;
    n++;
    i++;
  }
  if(n == 0)
    return parse_status::missing_numeral;
  if(n > 3)
    return parse_status::bad_numeral;
  const int d = numeral_index(std::string_view{buf, n});
  if(d < 0)
    return parse_status::bad_numeral;
  degree = int8_t(d);
  lower = s[start] == 'i' || s[start] == 'v';
  return parse_status::ok;
}
}

//! Parses one symbol (no spaces). The error position is relative to `s`.
inline constexpr parse_error parse_symbol(std::string_view s, chord_symbol& out) noexcept
{
  out = {};
  const auto fail = [&](parse_status st) {
    return parse_error{st, 0, uint16_t(std::min<std::size_t>(s.size(), 0xFFFF))};
  };
  if(s.empty())
    return fail(parse_status::missing_numeral);
  if(s == "X" || s == "x")
  {
    out.rest = true;
    return {};
  }

  const std::size_t slash = s.find('/');
  const std::string_view head = s.substr(0, slash);

  std::size_t i = 0;
  if(auto st = detail::read_degree(head, i, out.degree, out.accidental, out.lower);
     st != parse_status::ok)
    return fail(st);

  const std::string_view suffix = head.substr(i);
  if(suffix == "7")
    out.seventh = true;
  else if(!suffix.empty())
  {
    out.type = find_suffix(suffix);
    if(!out.type)
      return fail(parse_status::unknown_suffix);
  }

  if(slash != std::string_view::npos)
  {
    const std::string_view tail = s.substr(slash + 1);
    std::size_t j = 0;
    bool lower{};
    if(detail::read_degree(tail, j, out.bass_degree, out.bass_accidental, lower)
       != parse_status::ok)
      return fail(parse_status::bad_bass);
    const std::string_view bsuf = tail.substr(j);
    if(!bsuf.empty() && bsuf != "7" && !find_suffix(bsuf))
      return fail(parse_status::bad_bass);
    out.slash = true;
  }
  return {};
}

/**
 * @brief Parses a whole progression, e.g. "I V vi IV" or "ii7 V7 I7  I".
 *
 * Never allocates. On error, `out` holds the symbols before the bad one and
 * the error gives the bad token's position and length in `text`.
 */
inline constexpr parse_error
parse_progression(std::string_view text, parsed_progression& out) noexcept
{
  out.count = 0;
  if(auto eq = text.find(" ="); eq != std::string_view::npos)
    text = text.substr(0, eq);
  if(text.size() > 0xFFFF)
    text = text.substr(0, 0xFFFF);

  std::size_t i = 0;
  while(i < text.size() && text[i] == ' ')
    i++;

  const auto push = [&](const chord_symbol& c) {
    if(out.count >= parsed_progression::max_chords)
      return false;
    out.chords[out.count++] = c;
    return true;
  };

  while(i < text.size())
  {
    const std::size_t start = i;
    while(i < text.size() && text[i] != ' ')
      i++;
    const std::string_view tok = text.substr(start, i - start);

    chord_symbol c;
    if(auto err = parse_symbol(tok, c))
    {
      err.position = uint16_t(start + err.position);
      return err;
    }
    c.position = uint16_t(start);
    c.length = uint16_t(tok.size());
    if(!push(c))
      return {parse_status::too_many, uint16_t(start), uint16_t(tok.size())};

    std::size_t spaces = 0;
    while(i < text.size() && text[i] == ' ')
    {
      i++;
      spaces++;
    }
    if(i < text.size())
    {
      for(std::size_t k = 1; k < spaces; k++)
      {
        chord_symbol r;
        r.rest = true;
        r.position = uint16_t(i - spaces + k);
        r.length = 0;
        if(!push(r))
          return {parse_status::too_many, r.position, 1};
      }
    }
  }
  if(out.count == 0)
    return {parse_status::empty, 0, 0};
  return {};
}

/**
 * @brief Writes "Unknown chord type: IVxx (at 5)" into `buf`, truncated to its
 * size. Returns the length written.
 */
inline std::size_t format_parse_error(
    const parse_error& e, std::string_view text, std::span<char> buf) noexcept
{
  std::size_t n = 0;
  const auto put = [&](std::string_view s) {
    for(char c : s)
      if(n < buf.size())
        buf[n++] = c;
  };
  put(parse_status_text(e.status));
  if(e.status == parse_status::ok || e.status == parse_status::empty)
    return n;
  if(e.position < text.size())
  {
    put(": ");
    put(text.substr(e.position, std::max<std::size_t>(e.length, 1)));
  }
  put(" (at ");
  char num[8]{};
  int k = 0;
  int v = e.position;
  do
  {
    num[k++] = char('0' + v % 10);
    v /= 10;
  } while(v > 0 && k < 7);
  while(k > 0)
    put(std::string_view{&num[--k], 1});
  put(")");
  return n;
}

// ---------------------------------------------------------------- realize

//! A chord in a key: root pitch class and intervals from the root.
struct realized_chord
{
  bool rest{true};
  int8_t root{};   //!< pitch class, 0 - 11
  int8_t bass{-1}; //!< pitch class of a slash chord's bass, or -1
  int8_t count{};
  std::array<int8_t, 6> intervals{}; //!< ascending, the root first

  std::span<const int8_t> notes() const noexcept
  {
    return {intervals.data(), std::size_t(count)};
  }

  bool has_pitch_class(int pc) const noexcept
  {
    for(int i = 0; i < count; i++)
      if((root + intervals[i]) % 12 == ((pc % 12) + 12) % 12)
        return true;
    return false;
  }
};

inline constexpr int pitch_class(int p) noexcept
{
  return ((p % 12) + 12) % 12;
}

//! The diatonic chord of `degree` in a mode, `size` notes stacked in thirds.
inline constexpr realized_chord diatonic_chord(mode m, int key_root, int degree, int size) noexcept
{
  realized_chord r;
  r.rest = false;
  const int base = degree_to_semitones(m, degree);
  r.root = int8_t(pitch_class(key_root + base));
  r.count = int8_t(std::clamp(size, 1, 6));
  for(int k = 0; k < r.count; k++)
    r.intervals[k] = int8_t(degree_to_semitones(m, degree + 2 * k) - base);
  return r;
}

/**
 * @brief The notes of a symbol in a key, following mingus' to_chords (see the
 * rules at the top of this file).
 */
inline constexpr realized_chord
realize(const chord_symbol& s, int key_root, mode m) noexcept
{
  if(s.rest)
    return {};
  realized_chord r;
  if(s.type)
  {
    r.rest = false;
    r.root = int8_t(pitch_class(key_root + degree_to_semitones(m, s.degree)));
    r.count = s.type->count;
    for(int k = 0; k < r.count; k++)
      r.intervals[k] = s.type->intervals[k];
  }
  else
  {
    r = diatonic_chord(m, key_root, s.degree, s.seventh ? 4 : 3);
  }
  r.root = int8_t(pitch_class(r.root + s.accidental));
  if(s.slash)
  {
    const int b
        = pitch_class(key_root + degree_to_semitones(m, s.bass_degree) + s.bass_accidental);
    if(b != r.root)
      r.bass = int8_t(b);
  }
  return r;
}

// ------------------------------------------------------------------ names

//! Whether a key's chords read better with flats: a key signature with flats
//! (or C, whose chromatic chords are mostly flat degrees).
inline constexpr bool key_prefers_flats(int key_root, mode m) noexcept
{
  constexpr int tonic_degree[9]{0, 1, 2, 3, 4, 5, 6, 5, 5};
  const int parent = pitch_class(key_root - mode_steps[0][tonic_degree[int(m)]]);
  switch(parent)
  {
    case 7:
    case 2:
    case 9:
    case 4:
    case 11:
    case 6:
      return false;
    default:
      return true;
  }
}

inline constexpr std::string_view note_name(int pc, bool flats) noexcept
{
  constexpr std::string_view sharp[12]{"C",  "C#", "D",  "D#", "E",  "F",
                                       "F#", "G",  "G#", "A",  "A#", "B"};
  constexpr std::string_view flat[12]{"C",  "Db", "D",  "Eb", "E",  "F",
                                      "Gb", "G",  "Ab", "A",  "Bb", "B"};
  return flats ? flat[pitch_class(pc)] : sharp[pitch_class(pc)];
}

//! A name for a set of intervals: Theory.hpp's chord type names first (the
//! major triad shown as nothing), then mingus' suffixes; "?" if none fits.
inline constexpr std::string_view chord_suffix_name(std::span<const int8_t> iv) noexcept
{
  const auto same = [&](const chord_type& c) {
    if(std::size_t(c.count) != iv.size())
      return false;
    for(std::size_t i = 0; i < iv.size(); i++)
      if(c.intervals[i] != iv[i])
        return false;
    return true;
  };
  for(const auto& c : chord_types)
    if(same(c))
      return c.name == "M" ? std::string_view{} : c.name;
  for(const auto& c : mingus_suffixes)
    if(same(c))
      return c.name;
  return "?";
}

//! "Am7", "C/Bb", "Ebmaj7" into `buf`. Returns the length written, 0 for a rest.
inline std::size_t
format_chord_name(const realized_chord& c, bool flats, std::span<char> buf) noexcept
{
  std::size_t n = 0;
  const auto put = [&](std::string_view s) {
    for(char ch : s)
      if(n < buf.size())
        buf[n++] = ch;
  };
  if(c.rest)
    return 0;
  put(note_name(c.root, flats));
  put(chord_suffix_name(c.notes()));
  if(c.bass >= 0)
  {
    put("/");
    put(note_name(c.bass, flats));
  }
  return n;
}

// ---------------------------------------------------------------- voicing

enum class inversion_mode : uint8_t
{
  root,   //!< root position (a slash chord's bass lowest when it is a chord tone)
  closest //!< the inversion closest to the previous chord
};

struct voicing_options
{
  int center{60};      //!< the register: the chord's mean pitch stays near it
  inversion_mode inversion{inversion_mode::closest};
  bool bass{};         //!< add the bass (root, or slash bass) an octave below
  int max_notes{6};    //!< chord tones kept, the bass not counted
};

//! Pitches of a voiced chord, ascending.
struct voicing
{
  static constexpr int max_notes = 8;
  std::array<uint8_t, max_notes> notes{};
  int8_t count{};
  bool has_bass{}; //!< notes[0] is the added bass note

  std::span<const uint8_t> all() const noexcept
  {
    return {notes.data(), std::size_t(count)};
  }
  std::span<const uint8_t> upper() const noexcept
  {
    const int b = has_bass && count > 0 ? 1 : 0;
    return {notes.data() + b, std::size_t(count - b)};
  }
};

namespace detail
{
//! Total movement between two chords: each note to its nearest note of the
//! other chord, both ways, so that chords of different sizes compare.
inline int voice_movement(std::span<const uint8_t> a, std::span<const uint8_t> b) noexcept
{
  int cost = 0;
  for(int x : a)
  {
    int best = 128;
    for(int y : b)
      best = std::min(best, std::abs(x - y));
    cost += best;
  }
  for(int y : b)
  {
    int best = 128;
    for(int x : a)
      best = std::min(best, std::abs(x - y));
    cost += best;
  }
  return cost;
}

inline void insert_sorted(voicing& v, int pitch) noexcept
{
  if(pitch < 0 || pitch > 127 || v.count >= voicing::max_notes)
    return;
  int i = v.count;
  while(i > 0 && v.notes[i - 1] > pitch)
  {
    v.notes[i] = v.notes[i - 1];
    i--;
  }
  v.notes[i] = uint8_t(pitch);
  v.count++;
}
}

/**
 * @brief Voices a chord near `o.center`, as close as possible to `previous`.
 *
 * Tones: with fewer than all allowed, the root and the third (second
 * interval) stay, then the top tones (seventh, extensions), the fifth goes
 * first. Inversion k puts the k-th tone lowest and the ones under it an octave
 * up, in close position (ninths and above stay compound in root position).
 *
 * Root mode, and the first chord in closest mode: root position, at the
 * octave whose mean pitch is nearest the center.
 * Closest mode: among the inversions whose mean is within 6 semitones of the
 * center (so the register never drifts), the one with the least total
 * movement from `previous`'s chord tones; ties go to the one nearer the
 * center.
 * A slash chord's bass is the lowest note: the inversion that starts on it if
 * it is a chord tone, else an added note under the chord. With `o.bass`, the
 * bass (the slash bass, else the root) is added at most an octave below the
 * center instead, and is notes[0].
 */
inline voicing
voice(const realized_chord& c, const voicing& previous, const voicing_options& o) noexcept
{
  voicing res;
  if(c.rest || c.count <= 0)
    return res;

  const int n = c.count;
  const int keep = std::clamp(o.max_notes, 1, n);
  std::array<int, 6> tones{};
  {
    int order[6]{};
    int k = 0;
    order[k++] = 0;
    if(n > 1)
      order[k++] = 1;
    for(int i = n - 1; i >= 2; i--)
      order[k++] = i;
    for(int i = 0; i < keep; i++)
      tones[i] = c.intervals[order[i]];
    std::sort(tones.begin(), tones.begin() + keep);
  }

  const int slash_pc = c.bass;
  int lowest_pc = -1;
  bool slash_below = false;
  if(slash_pc >= 0 && !o.bass)
  {
    bool in_chord = false;
    for(int i = 0; i < keep; i++)
      if(pitch_class(c.root + tones[i]) == slash_pc)
        in_chord = true;
    if(in_chord)
      lowest_pc = slash_pc;
    else
      slash_below = true;
  }

  const auto prev = previous.upper();
  const bool by_movement = o.inversion == inversion_mode::closest && !prev.empty();

  struct candidate
  {
    std::array<uint8_t, 6> p{};
    double dist{1e9};
    int cost{1 << 30};
    bool valid{};
  };
  candidate best;
  candidate best_any;

  for(int inv = 0; inv < keep; inv++)
  {
    if(!by_movement && lowest_pc < 0 && inv != 0)
      continue;
    std::array<int, 6> rel{};
    for(int i = 0; i < keep; i++)
      rel[i] = i < inv ? tones[i] + 12 : tones[i];
    std::sort(rel.begin(), rel.begin() + keep);
    if(lowest_pc >= 0 && pitch_class(c.root + rel[0]) != lowest_pc)
      continue;

    for(int oct = -1; oct <= 10; oct++)
    {
      const int base = c.root + 12 * oct;
      if(base + rel[0] < 0 || base + rel[keep - 1] > 127)
        continue;
      candidate cand;
      double sum = 0.;
      for(int i = 0; i < keep; i++)
      {
        cand.p[i] = uint8_t(base + rel[i]);
        sum += cand.p[i];
      }
      cand.dist = std::abs(sum / keep - o.center);
      cand.valid = true;
      if(by_movement)
        cand.cost = detail::voice_movement({cand.p.data(), std::size_t(keep)}, prev);
      else
        cand.cost = 0;

      const auto better = [](const candidate& a, const candidate& b) {
        if(!b.valid)
          return true;
        if(a.cost != b.cost)
          return a.cost < b.cost;
        return a.dist < b.dist - 1e-9;
      };
      if(cand.dist <= 6. + 1e-9 && better(cand, best))
        best = cand;
      // Fallback when no candidate sits in the window: nearest the center.
      if(!best_any.valid || cand.dist < best_any.dist - 1e-9)
        best_any = cand;
    }
  }
  if(!best.valid)
    best = best_any;

  if(best.valid)
  {
    for(int i = 0; i < keep; i++)
      res.notes[i] = best.p[i];
    res.count = int8_t(keep);
  }
  else
  {
    // Center at the edge of the MIDI range: root position, what fits.
    const int base = c.root + 12 * ((o.center - c.root) / 12);
    for(int i = 0; i < keep; i++)
      detail::insert_sorted(res, base + tones[i]);
  }
  if(res.count == 0)
    return res;

  if(o.bass)
  {
    const int pc = slash_pc >= 0 ? slash_pc : c.root;
    int b = o.center - 12;
    while(b >= 0 && pitch_class(b) != pc)
      b--;
    while(b >= 0 && b >= res.notes[0])
      b -= 12;
    if(b >= 0 && res.count < voicing::max_notes)
    {
      detail::insert_sorted(res, b);
      res.has_bass = true;
    }
  }
  else if(slash_below)
  {
    int b = res.notes[0] - 1;
    while(b >= 0 && pitch_class(b) != slash_pc)
      b--;
    if(b >= 0)
      detail::insert_sorted(res, b);
  }
  return res;
}

// ------------------------------------------------------------ conforming

//! The pitch nearest `pitch` whose pitch class `ok(pc)` accepts, ties going
//! down, within 0 - 127; `pitch` itself if none within an octave.
template <typename F>
inline int nearest_pitch(int pitch, F&& ok) noexcept
{
  for(int d = 0; d <= 12; d++)
  {
    if(pitch - d >= 0 && ok(pitch_class(pitch - d)))
      return pitch - d;
    if(d > 0 && pitch + d <= 127 && ok(pitch_class(pitch + d)))
      return pitch + d;
  }
  return pitch;
}

//! Nearest tone of the chord, ties going down.
inline int nearest_chord_tone(int pitch, const realized_chord& c) noexcept
{
  if(c.rest)
    return pitch;
  return nearest_pitch(pitch, [&](int pc) { return c.has_pitch_class(pc); });
}

//! Nearest tone of the scale, ties going down.
inline int nearest_scale_tone(int pitch, int key_root, mode m) noexcept
{
  return nearest_pitch(pitch, [&](int pc) {
    for(int s : mode_steps[int(m)])
      if(pitch_class(key_root + s) == pc)
        return true;
    return false;
  });
}

/**
 * @brief Tone `index` of the chord counted over octaves: 0 is the root, 1 the
 * next tone up, and past the last tone the root an octave higher. Negative
 * indices go down. `root_pitch` is the pitch of tone 0.
 */
inline int chord_tone(const realized_chord& c, int root_pitch, int index) noexcept
{
  if(c.rest || c.count <= 0)
    return root_pitch;
  std::array<int, 6> pcs{};
  int n = 0;
  for(int i = 0; i < c.count; i++)
  {
    const int r = c.intervals[i] % 12;
    bool dup = false;
    for(int k = 0; k < n; k++)
      dup |= pcs[k] == r;
    if(!dup)
      pcs[n++] = r;
  }
  std::sort(pcs.begin(), pcs.begin() + n);
  const int oct = index >= 0 ? index / n : -((-index + n - 1) / n);
  const int k = index - oct * n;
  return root_pitch + 12 * oct + pcs[k];
}
}
