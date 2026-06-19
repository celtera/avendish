#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Annotation tag carrying a (key, value) metadata pair, e.g.:
//   [[=halp::meta("author", "Jean-Michael Celerier")]]
//   [[=halp::meta("category", "Filters")]]
// Reflection analogue of the halp_meta(name, value) macro; re-exported as
// halp::meta().

#include <cstddef>
#include <string_view>

namespace avnd
{
template <std::size_t KN, std::size_t VN>
struct metadata_attribute
{
  char key_[KN]{};
  char value_[VN]{};
  consteval metadata_attribute(const char (&k)[KN], const char (&v)[VN]) noexcept
  {
    for(std::size_t i = 0; i < KN; ++i)
      key_[i] = k[i];
    for(std::size_t i = 0; i < VN; ++i)
      value_[i] = v[i];
  }
  constexpr std::string_view key() const noexcept { return {key_, KN - 1}; }
  constexpr std::string_view value() const noexcept { return {value_, VN - 1}; }
};
}
