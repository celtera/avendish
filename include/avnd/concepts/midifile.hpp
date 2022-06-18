#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/concepts/file_port.hpp>

namespace avnd
{

template <typename T>
concept midifile = file<T>
&& requires(T t)
{
  t.tracks[0][0].bytes;
  t.tracks[0][0].tick;
};

template <typename T>
concept midifile_port = midifile<std::decay_t<decltype(std::declval<T>().midifile)>>;

}
