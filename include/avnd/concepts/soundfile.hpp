#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{

template <typename T>
concept soundfile = requires(T t)
{
  t.data;
  t.frames;
  t.channels;
};

template <typename T>
concept soundfile_port = soundfile<std::decay_t<decltype(std::declval<T>().soundfile)>>;

}
