#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/concepts/midi.hpp>

namespace avnd
{

// Note ports
// FIXME MPE, etc.
template <typename T>
concept note_port = requires(T t) { t.notes; };

template <typename T>
concept dynamic_container_notes = note_port<T> && vector_ish<decltype(T::midi_messages)>;

template <typename T>
concept raw_container_note_port
    = note_port<T> && std::is_pointer_v<decltype(T::midi_messages)>
      && std::is_integral_v<decltype(T::size)>;

}
