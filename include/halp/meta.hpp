#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#define halp_meta(name, value) \
  static consteval auto name() \
  {                            \
    return value;              \
  }

#define halp_flag(flag) \
  enum : bool           \
  {                     \
    flag                \
  }
#define halp_flags(...) \
  enum : unsigned char  \
  {                     \
    __VA_ARGS__         \
  }

#if defined(_MSC_VER)
#define HALP_RESTRICT __restrict
#else
#define HALP_RESTRICT __restrict__
#endif

#include <avnd/common/metadata_tag.hpp>
#include <halp/modules.hpp>

#include <cstddef>
#include <type_traits>

HALP_MODULE_EXPORT
namespace halp
{
// C++26 reflection annotation analogue of the halp_meta(name, value) macro:
//   [[=halp::meta("category", "Filters")]] struct MyObject { ... };
// Read generically with avnd::annotated_metadata<T>("category").
template <std::size_t KN, std::size_t VN>
consteval auto meta(const char (&key)[KN], const char (&value)[VN]) noexcept
{
  return avnd::metadata_attribute<KN, VN>{key, value};
}

// Extracts the type of x in &Class::x
template <typename T>
struct member_type_extractor;

template <typename C, typename T>
struct member_type_extractor<T C::*>
{
  using type = T;
};

template <typename T>
using member_type_t = typename member_type_extractor<std::remove_cvref_t<T>>::type;
}
