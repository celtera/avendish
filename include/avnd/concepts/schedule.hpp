#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <chrono>
#include <type_traits>

// clang-format off
namespace avnd
{
/**
 * "schedule" concept: used to give access to a schedule / defer_low-like API
 */
type_or_value_qualification(schedule)
type_or_value_reflection(schedule)
type_or_value_accessors(schedule)

/**
 * "clock" concept: will create a clock that regularly polls. Only way to handle 
 * messages from another thread in pd...
 */

define_get_property(clock, std::chrono::seconds, std::chrono::seconds(1))


template<typename T, typename Self>
concept clock_spec = requires (T t) {
  t.start = [] (auto time) { };
  t.timeout();
  t.stop();
};

type_or_value_qualification(clocks)
type_or_value_reflection(clocks)
type_or_value_accessors(clocks)
}
// clang-format on
