#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// P2996 backend: derive field names from member identifiers.

#include <avnd/common/meta_polyfill.hpp>

#if AVND_USE_STD_REFLECTION

#include <array>
#include <cstddef>
#include <string_view>

namespace avnd
{
template <typename T>
consteval std::size_t reflected_field_count() noexcept
{
  return std::meta::nonstatic_data_members_of(
             ^^T, std::meta::access_context::unchecked())
      .size();
}

// Non-static data member names of T, in declaration order.
template <typename T>
consteval std::array<std::string_view, reflected_field_count<T>()>
reflected_field_names() noexcept
{
  std::array<std::string_view, reflected_field_count<T>()> out{};
  std::size_t i = 0;
  for(std::meta::info m :
      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
    out[i++] = std::string_view{std::define_static_string(std::meta::identifier_of(m))};
  return out;
}
}

#endif
