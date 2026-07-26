#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/function_reflection.hpp>
#include <avnd/concepts/parameter.hpp>

namespace avnd
{
// enum-ish parameter carrying an update_items callback for runtime item lists.
// Any function-like member works (std::function-ish object, function pointer,
// ...); the argument type is left to the binding.
template <typename T>
concept dynamic_items_parameter
    = enum_ish_parameter<T>
      && (function_ish<std::decay_t<decltype(T{}.update_items)>>
          || function<std::decay_t<decltype(T{}.update_items)>>);
}
