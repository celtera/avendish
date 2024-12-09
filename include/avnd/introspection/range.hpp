#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/range.hpp>

namespace avnd
{

/// Range reflection ///
template <avnd::has_range T>
consteval auto get_range()
{
  if constexpr(requires { sizeof(typename T::range); })
    return typename T::range{};
  else if constexpr(requires { T::range(); })
    return T::range();
  else if constexpr(requires { sizeof(decltype(T::range)); })
    return T::range;
  else
    return T::there_is_no_range_here;
}

template <typename T>
consteval auto get_range(const T&)
{
  return get_range<T>();
}

template <avnd::has_visual_range T>
consteval auto get_visual_range()
{
  if constexpr(requires { sizeof(typename T::visual_range); })
    return typename T::visual_range{};
  else if constexpr(requires { T::visual_range(); })
    return T::visual_range();
  else if constexpr(requires { sizeof(decltype(T::visual_range)); })
    return T::visual_range;
  else
    return get_range<T>();
}

template <avnd::has_range T>
  requires(!avnd::has_visual_range<T>)
consteval auto get_visual_range()
{
  return get_range<T>();
}

template <typename T>
consteval auto get_visual_range(const T&)
{
  return get_visual_range<T>();
}
}
