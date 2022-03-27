#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{

// MIDI messages
template <typename T>
concept midi_message = requires(T t)
{
  t.bytes;
  t.timestamp;
};

template <typename T>
concept dynamic_midi_message = midi_message<T> && vector_ish<decltype(T::bytes)>;

template <typename T>
concept raw_midi_message = midi_message<T> && array_ish<decltype(T::bytes), 3>;

}
