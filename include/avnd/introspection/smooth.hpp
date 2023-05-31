#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/smooth.hpp>

namespace avnd
{
/// smooth reflection ///
template <avnd::has_smooth T>
consteval auto get_smooth()
{
    if constexpr(requires { sizeof(typename T::smooth); })
        return typename T::smooth{};
    else if constexpr(requires { T::smooth(); })
        return T::smooth();
    else if constexpr(requires { sizeof(decltype(T::smooth)); })
        return T::smooth;
    else
        return T::there_is_no_smooth_here;
}

template <typename T>
consteval auto get_smooth(const T&)
{
    return get_smooth<T>();
}

}
