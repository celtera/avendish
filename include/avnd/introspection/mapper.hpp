#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/mapper.hpp>

namespace avnd
{
/// Mapper reflection ////
// Used to define how UI sliders behave.
template <avnd::has_mapper T>
consteval auto get_mapper()
{
    if constexpr(requires { sizeof(typename T::mapper); })
    {
        static_assert(mapper<typename T::mapper>);
        return typename T::mapper{};
    }
    else if constexpr(requires { T::mapper(); })
    {
        static_assert(mapper<std::decay_t<decltype(T::mapper())>>);
        return T::mapper();
    }
    else if constexpr(requires { sizeof(decltype(T::mapper)); })
    {
        static_assert(mapper<std::decay_t<decltype(T::mapper)>>);
        return T::mapper;
    }
}

template <typename T>
consteval auto get_mapper(const T&)
{
    return get_mapper<T>();
}

}
