#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/concepts/file_port.hpp>

#include <iterator>

namespace avnd
{

template <typename T>
concept midi_event =
requires (T t) {
  std::begin(t.bytes);
  std::end(t.bytes);
  { t.bytes[0] = 0 } -> std::convertible_to<unsigned char>;
};

template <typename T>
concept midifile = file<T>
&& requires(T t)
{
  { t.tracks[0][0] } -> midi_event;
};

template <typename T>
concept midifile_port = midifile<std::decay_t<decltype(std::declval<T>().midifile)>>;

}
