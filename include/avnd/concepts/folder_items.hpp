#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/concepts/parameter.hpp>

#include <string_view>

namespace avnd
{
// A combobox whose items are the files of a sibling folder port, populated by
// the host at edit time. The port exposes folder_port() (the sibling port's
// name) and extensions() (accepted suffixes).
template <typename T>
concept folder_items_parameter = enum_ish_parameter<T> && requires {
  { T::folder_port() } -> std::convertible_to<std::string_view>;
  { T::extensions() } -> std::convertible_to<std::string_view>;
};
}
