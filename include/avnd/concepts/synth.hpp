#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <cinttypes>
#include <type_traits>
#include <utility>

namespace avnd
{

template <typename FP, typename T>
concept synth_processor = requires(
    T& t) { std::declval<T::voice>().operator()(t, (FP**)nullptr, (int32_t)0); };

}
