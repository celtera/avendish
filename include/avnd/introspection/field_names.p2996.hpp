#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// C++26 reflection backend: derive a struct's field names directly from its
// member identifiers, so the halp_field_names(...) macro becomes optional.
// boost.pfr / P1061 structured-binding packs cannot recover member names; this
// is a genuine capability only static reflection provides.

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

// Names of the non-static data members of T, in declaration order. Backed by
// NUL-terminated static storage (define_static_string), matching the macro.
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
