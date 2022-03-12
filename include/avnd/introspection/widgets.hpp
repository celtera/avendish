#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <type_traits>

namespace avnd
{
/// Widget reflection ///
template <typename C>
concept has_widget = requires { std::is_enum_v<typename C::widget>; };

/// Range reflection ///
template <typename C>
concept has_range = requires { C::range(); } || requires { sizeof(C::range); }
                    || requires { sizeof(typename C::range); };

template <typename T>
consteval auto get_range()
{
  if constexpr (requires { sizeof(typename T::range); })
    return typename T::range{};
  else if constexpr (requires { T::range(); })
    return T::range();
  else if constexpr (requires { sizeof(decltype(T::range)); })
    return T::range;
  else
  {
    using value_type = std::decay_t<decltype(T::value)>;
    struct dummy_range
    {
      value_type min;
      value_type max;
      value_type init;
    };
    dummy_range r;
    r.min = 0.;
    r.max = 1.;
    r.init = 0.5;
    return r;
  }
}

template <typename T>
consteval auto get_range(const T&)
{
  return get_range<T>();
}

/// Normalization reflection ////
template <typename C>
concept has_normalize
    = requires { C::normalize(); } || requires { sizeof(C::normalize); }
      || requires { sizeof(typename C::normalize); };

template <avnd::has_normalize T>
consteval auto get_normalize()
{
  if constexpr (requires { sizeof(typename T::normalize); })
    return typename T::normalize{};
  else if constexpr (requires { T::normalize(); })
    return T::normalize();
  else if constexpr (requires { sizeof(decltype(T::normalize)); })
    return T::normalize;
}

template <typename T>
consteval auto get_normalize(const T&)
{
  return get_normalize<T>();
}
}
