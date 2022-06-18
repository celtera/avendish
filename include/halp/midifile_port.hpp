#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/inline.hpp>
#include <halp/midi.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <cstddef>
#include <string_view>
#include <vector>

namespace halp
{
struct midi_track_event
{
  boost::container::small_vector<uint8_t, 15> bytes;
  int tick_delta = 0;
};

struct simple_midi_track_event
{
  uint8_t bytes[3];
  int64_t tick_absolute;
};

template<typename Event>
using midi_track = std::vector<Event>;

template<typename Event>
struct midifile_view {
  std::vector<midi_track<Event>> tracks;

  // Length in ticks and other timing-relevant properties
  int64_t length{};
  int64_t ticks_per_beat{};
  float starting_tempo{};

  // std::fs::path would be great but limits to macOS 10.15+
  std::string_view filename;
};

template <halp::static_string lit, typename Event = simple_midi_track_event>
struct midifile_port
{
  using midifile_view_type = midifile_view<Event>;
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  HALP_INLINE_FLATTEN operator midifile_view_type&() noexcept { return midifile; }
  HALP_INLINE_FLATTEN operator const midifile_view_type&() const noexcept { return midifile; }
  HALP_INLINE_FLATTEN operator bool() const noexcept { return midifile.tracks.size() > 0; }

  midifile_view_type midifile;
};

}
#include <avnd/concepts/midifile.hpp>
static_assert(avnd::midifile_port<halp::midifile_port<"foo">>);
