#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/concepts/midi.hpp>

namespace avnd
{

// MIDI ports
template <typename T>
concept midi_port = requires(T t) { t.midi_messages; };

template <typename T>
concept dynamic_container_midi_port
    = midi_port<T> && vector_ish<decltype(T::midi_messages)>;

template <typename T>
concept raw_container_midi_port
    = midi_port<T> && std::is_pointer_v<decltype(T::midi_messages)>
      && std::is_integral_v<decltype(T::size)>;

}
