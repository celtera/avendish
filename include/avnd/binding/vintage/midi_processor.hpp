#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/concepts.hpp>
#include <avnd/wrappers/midi_introspection.hpp>
#include <avnd/binding/vintage/vintage.hpp>

namespace vintage
{
template <typename T>
struct midi_processor
    : public avnd::midi_storage<T>
{
  void init_midi_message(
      avnd::dynamic_midi_message auto& in,
      const vintage::MidiEvent& msg)
  {
    using byte_type = typename decltype(in.bytes)::value_type;
    using bytes_type = const byte_type*;
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

    in.bytes = reinterpret_cast<bytes_type>(msg.midiData);
    in.size = 4;
    if_possible(in.timestamp = msg.deltaFrames);
  }

  void add_message(
      avnd::dynamic_container_midi_port auto& port,
      const vintage::MidiEvent& msg)
  {
    port.midi_messages.push_back({});
    auto& elt = port.midi_messages.back();
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
