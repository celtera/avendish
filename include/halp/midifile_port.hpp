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
  int tick = 0;
};

using midi_track = std::vector<midi_track_event>;

struct midifile_view {
  std::vector<midi_track> tracks;

  // Length in ticks
  int64_t get_length() const noexcept
  {
    int64_t total = 0;
    for (const auto& t : tracks)
    {
      int64_t localLength = 0;
      for (const auto& e : t)
        localLength += e.tick;

      if (localLength > total)
        total = localLength;
    }
    return total;
  }

  // std::fs::path would be great but limits to macOS 10.15+
  std::string_view filename;
};

template <halp::static_string lit>
struct midifile_port
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  HALP_INLINE_FLATTEN operator midifile_view&() noexcept { return midifile; }
  HALP_INLINE_FLATTEN operator const midifile_view&() const noexcept { return midifile; }
  //HALP_INLINE_FLATTEN operator bool() const noexcept { return soundfile.data && soundfile.channels > 0 && soundfile.frames > 0; }

  midifile_view midifile;
};
}
#include <avnd/concepts/midifile.hpp>
static_assert(avnd::midifile_port<halp::midifile_port<"foo">>);
