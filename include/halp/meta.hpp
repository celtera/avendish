#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#define halp_meta(name, value) \
  static consteval auto name() \
  {                            \
    return value;              \
  }

#define halp_flag(flag) \
  enum                  \
  {                     \
    flag                \
  }
#define halp_flags(...) \
  enum                  \
  {                     \
    __VA_ARGS__         \
  }

#if defined(_MSC_VER)
#define HALP_RESTRICT __restrict
#else
#define HALP_RESTRICT __restrict__
#endif

#include <type_traits>
namespace halp
{
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
