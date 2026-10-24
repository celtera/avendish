#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
#include <avnd/introspection/midi.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <libremidi/ump.hpp>

#include <array>
#include <vector>

namespace oscr
{
// A node runs once per token it is given, and a buffer can hold several:
// a process looping inside the buffer runs up to its loop point then from its
// start, a process stopped inside it runs its last partial tick then an empty
// one. The MIDI outlets keep what all the runs of a buffer sent, unless the
// object sets local_midi_tick_batch to false: each run then replaces what the
// previous one sent.
template <typename T>
inline constexpr bool use_local_midi_tick_batch = [] {
  if constexpr(requires { T::local_midi_tick_batch(); })
    return bool(T::local_midi_tick_batch());
  else if constexpr(requires { T::local_midi_tick_batch; })
    return bool(T::local_midi_tick_batch);
  else
    return avnd::annotated_or_empty<T>("local_midi_tick_batch") != "false";
}();

// Packets per MIDI output reserved for a buffer: local_midi_tick_batch_reserve,
// or the buffer size.
template <typename T>
std::size_t local_midi_tick_batch_reserve(std::size_t buffer_size) noexcept
{
  if constexpr(requires { T::local_midi_tick_batch_reserve(); })
    return T::local_midi_tick_batch_reserve();
  else if constexpr(requires { T::local_midi_tick_batch_reserve; })
    return T::local_midi_tick_batch_reserve;
  else
    return buffer_size;
}

// Already-emitted packets for one graph execution, not pending processor events.
template <typename T>
struct current_buffer_midi
{
  static constexpr auto port_count
      = use_local_midi_tick_batch<T> ? avnd::midi_output_introspection<T>::size : 0;
  std::array<std::vector<libremidi::ump>, port_count> packets;

  void clear() noexcept
  {
    for(auto& port : packets)
      port.clear();
  }

  void reserve(std::size_t count)
  {
    for(auto& port : packets)
      port.reserve(count);
  }

  template <std::size_t Index>
  auto& messages() noexcept
  {
    constexpr auto index = avnd::midi_output_introspection<T>::field_index_to_index(
        avnd::field_index<Index>{});
    return packets[std::size_t(index)];
  }
};
}
