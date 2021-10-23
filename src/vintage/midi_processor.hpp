#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts.hpp>
#include <avnd/midi_introspection.hpp>
#include <vintage/vintage.hpp>

namespace vintage
{
template <typename T>
struct midi_processor
{
  [[no_unique_address]] avnd::midi_port_storage<T> midiPortStorage;

  void
  reserve_space(avnd::dynamic_container_midi_port auto& port, int buffer_size)
  {
    // Here we use the vector in the port directly.
    port.midi_messages.clear();
    port.midi_messages.reserve(buffer_size);
  }

  void reserve_space(avnd::raw_container_midi_port auto& port, int buffer_size)
  {
    // In this API we have a single port, so we can check index 0 directly...
    // We allocate some memory locally and save a pointer in the structure.
    auto& buf = get<0>(midiPortStorage.storage);
    buf.resize(buffer_size);

    port.midi_messages = buf.data();
    port.size = 0;
  }

  void clear(avnd::dynamic_container_midi_port auto& port)
  {
    port.midi_messages.clear();
  }

  void clear(avnd::raw_container_midi_port auto& port) { port.size = 0; }

  void init_midi_message(
      avnd::dynamic_midi_message auto& in,
      const vintage::MidiEvent& msg)
  {
    using bytes_type = decltype(in.bytes);
    static_assert(sizeof(in.bytes[0]) == sizeof(msg.midiData[0]));
    // todo: if the size is different, do a copy instead

    auto bytes = reinterpret_cast<bytes_type>(msg.midiData);
    in.bytes.assign(
        bytes, bytes + 4); // bytes per midi message fixed in this old api

    if_possible(in.timestamp = msg.deltaFrames);
  }

  void init_midi_message(
      avnd::raw_midi_message auto& in,
      const vintage::MidiEvent& msg)
  {
    using bytes_type = decltype(in.bytes);
    static_assert(sizeof(in.bytes[0]) == sizeof(msg.midiData[0]));

    in.bytes = reinterpret_cast<decltype(in.bytes)>(msg.midiData);
    in.size = 4;
    if_possible(in.timestamp = msg.deltaFrames);
  }

  void add_message(
      avnd::dynamic_container_midi_port auto& port,
      const vintage::MidiEvent& msg)
  {
    auto& elt = port.midi_messages.emplace_back({});
    init_midi_message(elt, msg);
  }

  void add_message(
      avnd::raw_container_midi_port auto& port,
      const vintage::MidiEvent& msg)
  {
    auto& elt = port.midi_messages[port.size];
    init_midi_message(elt, msg);

    port.size++;
  }
};
}
