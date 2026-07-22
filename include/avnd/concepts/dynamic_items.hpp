#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/parameter.hpp>

#include <functional>
#include <string>
#include <vector>

namespace avnd
{
// enum-ish parameter carrying an update_items callback for runtime item lists.
template <typename T>
concept dynamic_items_parameter = enum_ish_parameter<T> && requires(T t) {
  {
    t.update_items
  } -> std::convertible_to<std::function<void(std::vector<std::string>)>>;
};
}
