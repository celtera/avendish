#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts.hpp>
#include <avnd/input_introspection.hpp>
#include <boost/mp11.hpp>

namespace avnd
{

template <raw_container_midi_port Field>
using midi_message_type = std::remove_pointer_t<
    std::remove_reference_t<decltype(Field::midi_messages)>>;

template <typename T>
struct midi_port_storage
{
};

template <typename T>
requires(
    raw_container_midi_input_introspection<T>::size
    > 0) struct midi_port_storage<T>
{
  using midi_messages_tuple = filter_and_apply<
      midi_message_type,
      raw_container_midi_input_introspection,
      T>;
  using midi_messages_vectors
      = boost::mp11::mp_transform<std::vector, midi_messages_tuple>;

  midi_messages_vectors storage;
};


template <typename T>
struct midi_storage
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

  void clear(avnd::raw_container_midi_port auto& port)
  {
    port.size = 0;
  }
};
}
