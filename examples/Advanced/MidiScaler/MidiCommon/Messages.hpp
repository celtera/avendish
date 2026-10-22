#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <libremidi/message.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace mtk
{
//! Every kind of MIDI 1 message an object may want to select.
enum class midi_kind : uint8_t
{
  note_on,
  note_off,
  control_change,
  program_change,
  pitch_bend,
  channel_pressure,
  poly_pressure,
  sysex,
  time_code,
  song_position,
  song_select,
  tune_request,
  clock,
  start,
  continue_,
  stop,
  active_sensing,
  reset,
  other
};

inline constexpr int midi_kind_count = int(midi_kind::other) + 1;

//! A note-on with velocity 0 is a note-off.
inline midi_kind classify(const libremidi::message& m) noexcept
{
  if((m.size() == 0))
    return midi_kind::other;
  const uint8_t s = m.bytes[0];
  if(s < 0xF0)
  {
    switch(s & 0xF0)
    {
      case 0x80:
        return midi_kind::note_off;
      case 0x90:
        return (m.size() >= 3 && m.bytes[2] == 0) ? midi_kind::note_off
                                                  : midi_kind::note_on;
      case 0xA0:
        return midi_kind::poly_pressure;
      case 0xB0:
        return midi_kind::control_change;
      case 0xC0:
        return midi_kind::program_change;
      case 0xD0:
        return midi_kind::channel_pressure;
      case 0xE0:
        return midi_kind::pitch_bend;
      default:
        return midi_kind::other;
    }
  }
  switch(s)
  {
    case 0xF0:
      return midi_kind::sysex;
    case 0xF1:
      return midi_kind::time_code;
    case 0xF2:
      return midi_kind::song_position;
    case 0xF3:
      return midi_kind::song_select;
    case 0xF6:
      return midi_kind::tune_request;
    case 0xF8:
      return midi_kind::clock;
    case 0xFA:
      return midi_kind::start;
    case 0xFB:
      return midi_kind::continue_;
    case 0xFC:
      return midi_kind::stop;
    case 0xFE:
      return midi_kind::active_sensing;
    case 0xFF:
      return midi_kind::reset;
    default:
      return midi_kind::other;
  }
}

inline bool is_channel_message(const libremidi::message& m) noexcept
{
  return !(m.size() == 0) && m.bytes[0] >= 0x80 && m.bytes[0] < 0xF0;
}

inline bool is_note_on(const libremidi::message& m) noexcept
{
  return m.size() >= 3 && (m.bytes[0] & 0xF0) == 0x90 && m.bytes[2] != 0;
}

inline bool is_note_off(const libremidi::message& m) noexcept
{
  if(m.size() < 3)
    return false;
  const uint8_t t = m.bytes[0] & 0xF0;
  return t == 0x80 || (t == 0x90 && m.bytes[2] == 0);
}

//! 1 to 16, or 0 for system messages.
inline int channel_of(const libremidi::message& m) noexcept
{
  return is_channel_message(m) ? (m.bytes[0] & 0x0F) + 1 : 0;
}

inline void set_channel(libremidi::message& m, int channel) noexcept
{
  if(is_channel_message(m))
    m.bytes[0] = uint8_t((m.bytes[0] & 0xF0) | ((channel - 1) & 0x0F));
}

//! CC 120 (all sound off) and CC 123 (all notes off).
inline bool is_all_notes_off(const libremidi::message& m) noexcept
{
  return m.size() >= 3 && (m.bytes[0] & 0xF0) == 0xB0
         && (m.bytes[1] == 120 || m.bytes[1] == 123);
}

//! "C4" is 60, "C-1" is 0, "G9" is 127. Accepts '#' and 'b'.
//! Returns the note and the number of characters read.
inline std::optional<std::pair<int, std::size_t>>
parse_note_name(std::string_view s) noexcept
{
  if(s.empty())
    return std::nullopt;

  static constexpr int pcs[7] = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
  char c = s[0];
  if(c >= 'a' && c <= 'g')
    c = char(c - 'a' + 'A');
  if(c < 'A' || c > 'G')
    return std::nullopt;

  int pc = pcs[c - 'A'];
  std::size_t i = 1;
  if(i < s.size() && (s[i] == '#' || s[i] == 'b'))
  {
    pc += (s[i] == '#') ? 1 : -1;
    i++;
  }

  bool negative = false;
  if(i < s.size() && s[i] == '-')
  {
    negative = true;
    i++;
  }
  if(i >= s.size() || s[i] < '0' || s[i] > '9')
    return std::nullopt;

  int octave = 0;
  while(i < s.size() && s[i] >= '0' && s[i] <= '9')
  {
    octave = octave * 10 + (s[i] - '0');
    i++;
    if(octave > 20)
      return std::nullopt;
  }
  if(negative)
    octave = -octave;

  const int note = (octave + 1) * 12 + pc;
  if(note < 0 || note > 127)
    return std::nullopt;
  return std::make_pair(note, i);
}

/**
 * @brief A set of inclusive integer ranges written as text: "1-4, 6, 9-16".
 *
 * With note names enabled, bounds may also be written "C-1-B2, C4, F#5".
 * An empty text matches nothing. Parsing never allocates.
 */
struct range_list
{
  static constexpr int max_ranges = 16;

  struct range
  {
    int lo{}, hi{};
  };

  std::array<range, max_ranges> ranges{};
  int count{};

  bool contains(int v) const noexcept
  {
    for(int i = 0; i < count; i++)
      if(v >= ranges[i].lo && v <= ranges[i].hi)
        return true;
    return false;
  }

  bool empty() const noexcept { return count == 0; }

  //! On error, the list is left empty and the position of the faulty
  //! character is returned.
  std::optional<std::size_t>
  parse(std::string_view s, int min, int max, bool note_names = false) noexcept
  {
    count = 0;
    std::size_t i = 0;

    auto skip_spaces = [&] {
      while(i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        i++;
    };

    auto read_bound = [&]() -> std::optional<int> {
      skip_spaces();
      if(i >= s.size())
        return std::nullopt;
      if(s[i] >= '0' && s[i] <= '9')
      {
        int v = 0;
        while(i < s.size() && s[i] >= '0' && s[i] <= '9')
        {
          v = v * 10 + (s[i] - '0');
          if(v > 100000)
            return std::nullopt;
          i++;
        }
        return v;
      }
      if(note_names)
      {
        if(auto n = parse_note_name(s.substr(i)))
        {
          i += n->second;
          return n->first;
        }
      }
      return std::nullopt;
    };

    skip_spaces();
    if(i == s.size())
      return std::nullopt;

    while(true)
    {
      const std::size_t start = i;
      auto lo = read_bound();
      if(!lo)
      {
        count = 0;
        return std::max(start, i);
      }
      int hi = *lo;
      skip_spaces();
      if(i < s.size() && s[i] == '-')
      {
        i++;
        const std::size_t hpos = i;
        auto h = read_bound();
        if(!h)
        {
          count = 0;
          return hpos;
        }
        hi = *h;
      }
      if(*lo > hi)
        std::swap(*lo, hi);
      if(*lo < min || hi > max || count >= max_ranges)
      {
        count = 0;
        return start;
      }
      ranges[count++] = {*lo, hi};

      skip_spaces();
      if(i == s.size())
        return std::nullopt;
      if(s[i] != ',')
      {
        count = 0;
        return i;
      }
      i++;
    }
  }
};

/**
 * @brief Keeps a range_list in sync with a text control without allocating.
 *
 * The text is compared with the last one parsed, kept in a fixed buffer; it is
 * only parsed again when it changed.
 */
struct cached_range_list
{
  static constexpr std::size_t max_text = 256;

  range_list list;
  bool valid{true};

  const range_list& update(std::string_view text, int min, int max, bool notes = false) noexcept
  {
    if(text.size() > max_text)
      text = text.substr(0, max_text);
    if(m_parsed && text == std::string_view{m_text.data(), m_size} && min == m_min
       && max == m_max && notes == m_notes)
      return list;

    std::copy(text.begin(), text.end(), m_text.begin());
    m_size = text.size();
    m_min = min;
    m_max = max;
    m_notes = notes;
    m_parsed = true;
    valid = !list.parse(text, min, max, notes).has_value();
    return list;
  }

private:
  std::array<char, max_text> m_text{};
  std::size_t m_size{};
  int m_min{}, m_max{};
  bool m_notes{};
  bool m_parsed{};
};
}
