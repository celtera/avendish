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

}
