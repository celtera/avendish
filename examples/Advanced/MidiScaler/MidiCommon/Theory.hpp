#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace mtk
{
/**
 * @brief Chord types, as semitone intervals from the root.
 *
 * Beyond the triads, the set and the interval spellings are those of the
 * chord types of free-midi-chords by Ludovic Drolez
 * (https://github.com/ldrolez/free-midi-chords, chords.py), MIT licence:
 *
 *   Copyright (c) 2019-2026 Ludovic Drolez
 *   Permission is hereby granted, free of charge, to any person obtaining a
 *   copy of this software and associated documentation files (the
 *   "Software"), to deal in the Software without restriction, including
 *   without limitation the rights to use, copy, modify, merge, publish,
 *   distribute, sublicense, and/or sell copies of the Software, and to permit
 *   persons to whom the Software is furnished to do so, subject to the
 *   following conditions: The above copyright notice and this permission
 *   notice shall be included in all copies or substantial portions of the
 *   Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 *
 * Their "2" (same notes as add9) and "dim6" (listed with the notes of dim7)
 * are left out.
 */
struct chord_type
{
  std::string_view name;
  std::array<int8_t, 6> intervals;
  int8_t count;

  std::span<const int8_t> notes() const noexcept { return {intervals.data(), size_t(count)}; }
};

inline constexpr chord_type chord_types[] = {
    // Triads and power chord
    {"M", {0, 4, 7}, 3},
    {"m", {0, 3, 7}, 3},
    {"dim", {0, 3, 6}, 3},
    {"aug", {0, 4, 8}, 3},
    {"5", {0, 7}, 2},
    {"sus2", {0, 2, 7}, 3},
    {"sus4", {0, 5, 7}, 3},
    // With a major third
    {"6", {0, 4, 7, 9}, 4},
    {"7", {0, 4, 7, 10}, 4},
    {"7-5", {0, 4, 6, 10}, 4},
    {"7+5", {0, 4, 8, 10}, 4},
    {"7sus4", {0, 5, 7, 10}, 4},
    {"maj7", {0, 4, 7, 11}, 4},
    {"M7+5", {0, 4, 8, 11}, 4},
    {"add4", {0, 4, 5, 7}, 4},
    {"add9", {0, 4, 7, 14}, 4},
    {"sus4add9", {0, 5, 7, 14}, 4},
    {"add11", {0, 4, 7, 17}, 4},
    {"69", {0, 4, 7, 9, 14}, 5},
    {"9", {0, 4, 7, 10, 14}, 5},
    {"maj9", {0, 4, 7, 11, 14}, 5},
    {"9sus4", {0, 5, 7, 10, 14}, 5},
    {"7-9", {0, 4, 7, 10, 13}, 5},
    {"7+11", {0, 4, 7, 10, 18}, 5},
    // With a minor third
    {"m6", {0, 3, 7, 9}, 4},
    {"m7", {0, 3, 7, 10}, 4},
    {"m7-5", {0, 3, 6, 10}, 4},
    {"m7+5", {0, 3, 8, 10}, 4},
    {"dim7", {0, 3, 6, 9}, 4},
    {"mM7", {0, 3, 7, 11}, 4},
    {"madd4", {0, 3, 5, 7}, 4},
    {"madd9", {0, 3, 7, 14}, 4},
    {"m69", {0, 3, 7, 9, 14}, 5},
    {"m9", {0, 3, 7, 10, 14}, 5},
    {"m7b9b5", {0, 3, 6, 10, 13}, 5},
    {"m7add11", {0, 3, 7, 10, 17}, 5},
    {"mM7add11", {0, 3, 7, 11, 17}, 5},
};

inline constexpr int chord_type_count = int(std::size(chord_types));

inline constexpr const chord_type* find_chord_type(std::string_view name) noexcept
{
  for(const auto& c : chord_types)
    if(c.name == name)
      return &c;
  return nullptr;
}

//! Seven-note modes and minor scales, as semitones from the tonic.
enum class mode : uint8_t
{
  ionian,
  dorian,
  phrygian,
  lydian,
  mixolydian,
  aeolian,
  locrian,
  harmonic_minor,
  melodic_minor
};

inline constexpr std::array<std::array<int8_t, 7>, 9> mode_steps{{
    {0, 2, 4, 5, 7, 9, 11},
    {0, 2, 3, 5, 7, 9, 10},
    {0, 1, 3, 5, 7, 8, 10},
    {0, 2, 4, 6, 7, 9, 11},
    {0, 2, 4, 5, 7, 9, 10},
    {0, 2, 3, 5, 7, 8, 10},
    {0, 1, 3, 5, 6, 8, 10},
    {0, 2, 3, 5, 7, 8, 11},
    {0, 2, 3, 5, 7, 9, 11},
}};

//! Semitones above the tonic of a scale degree, any integer (negative goes
//! down, 7 is the octave).
inline constexpr int degree_to_semitones(mode m, int degree) noexcept
{
  const auto& s = mode_steps[int(m)];
  int octave = degree >= 0 ? degree / 7 : -((-degree + 6) / 7);
  int d = degree - octave * 7;
  return octave * 12 + s[d];
}

//! The degree whose pitch class is closest to `semitones` (above the tonic),
//! ties going down. Returns the degree, octaves included.
inline constexpr int semitones_to_degree(mode m, int semitones) noexcept
{
  const int octave = semitones >= 0 ? semitones / 12 : -((-semitones + 11) / 12);
  const int pc = semitones - octave * 12;
  const auto& s = mode_steps[int(m)];
  int best = 0;
  int best_dist = 99;
  for(int d = 0; d < 8; d++)
  {
    const int st = d < 7 ? s[d] : 12;
    const int dist = st > pc ? st - pc : pc - st;
    if(dist < best_dist)
    {
      best = d;
      best_dist = dist;
    }
  }
  return octave * 7 + best;
}
}
