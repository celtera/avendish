#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Field-name accessor: prefers explicit field_names(), else reflects names.

#include <avnd/common/meta_polyfill.hpp>
#include <avnd/concepts/field_names.hpp>

#if AVND_USE_STD_REFLECTION
#include <avnd/introspection/field_names.p2996.hpp>
#endif

#include <array>
#include <string_view>

namespace avnd
{
// True when field names are available for T (explicitly, or via reflection).
template <typename T>
concept has_any_field_names = requires { T::field_names(); }
#if AVND_USE_STD_REFLECTION
                              || std::is_aggregate_v<T>
#endif
    ;

template <typename T>
constexpr auto get_field_names()
{
  if constexpr(requires { T::field_names(); })
    return T::field_names();
#if AVND_USE_STD_REFLECTION
  else
    return reflected_field_names<T>();
#else
  else
    return std::array<std::string_view, 0>{};
#endif
}
}
