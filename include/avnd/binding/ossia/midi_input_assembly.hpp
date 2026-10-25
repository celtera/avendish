#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
#include <avnd/introspection/midi.hpp>
#include <libremidi/detail/conversion.hpp>

#include <array>

namespace oscr
{
// Bytes of the longest system-exclusive message an object taking MIDI 1 is
// given, F0 and F7 included; a longer one is dropped.
inline constexpr std::size_t midi_input_sysex_capacity = 4096;

// A MIDI inlet carries UMP: a system-exclusive message is a sequence of SysEx7
// packets, which can be spread over the runs of a buffer and over buffers. Each
// MIDI input keeps the message it is assembling between runs; one left
// unfinished is dropped when the next one starts.
template <typename T>
struct midi_input_assembly
{
  using stream = libremidi::ump_to_midi1_stream<midi_input_sysex_capacity>;
  std::array<stream, avnd::midi_input_introspection<T>::size> inputs;

  template <std::size_t Index>
  stream& input(avnd::field_index<Index>) noexcept
  {
    constexpr auto index = avnd::midi_input_introspection<T>::field_index_to_index(
        avnd::field_index<Index>{});
    return inputs[std::size_t(index)];
  }
};
}
