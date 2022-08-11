#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <utility>

namespace avnd
{
template <typename C, template <typename...> class T>
constexpr auto configure()
{
  static_assert(
      requires { std::declval<T<C>>(); },
      "A requested configuration option is not supported");
  if constexpr(requires { std::declval<T<C>>(); })
  {
    struct
    {
      using type = T<C>;
    } x;
    return x;
  }
}

template <typename C, class T>
constexpr auto configure()
{
  struct
  {
    using type = T;
  } x;
  return x;
}
}
