#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */
//! Note ! read the comment below #if (__cpp_lib_concepts < 202002L)

#include <concepts>
#include <version>

#include <type_traits>

#if defined(_LIBCPP_CONCEPTS)
#if (__cpp_lib_concepts < 202002L)
// Taken from libstdc++, and as such follows libstdc++ license
namespace std
{
template <typename T>
concept integral = std::is_integral_v<T>;

template <typename T>
concept floating_point = std::is_floating_point_v<T>;

template <typename T, typename U>
concept convertible_to = requires(T t, U u) { t = u; };

/// [concept.constructible], concept constructible_from
template <typename _Tp, typename... _Args>
concept constructible_from
    = std::is_nothrow_destructible_v<_Tp> && is_constructible_v<_Tp, _Args...>;

template <typename _Tp>
concept default_initializable = constructible_from<_Tp> && requires {
                                                             _Tp{};
                                                             (void)::new _Tp;
                                                           };

namespace __detail
{
template <typename _Tp>
using __cref = const remove_reference_t<_Tp>&;

template <typename _Tp>
concept __class_or_enum = is_class_v<_Tp> || is_union_v<_Tp> || is_enum_v<_Tp>;
} // namespace __detail

template <typename _Lhs, typename _Rhs>
concept assignable_from
    = is_lvalue_reference_v<_Lhs> && requires(_Lhs __lhs, _Rhs&& __rhs) {
                                       {
                                         __lhs = static_cast<_Rhs&&>(__rhs)
                                         } -> same_as<_Lhs>;
                                     };

template <typename _Tp>
concept destructible = is_nothrow_destructible_v<_Tp>;

template <typename _Tp>
concept move_constructible = constructible_from<_Tp, _Tp> && convertible_to<_Tp, _Tp>;

template <typename _Tp>
concept copy_constructible
    = move_constructible<
          _Tp> && constructible_from<_Tp, _Tp&> && convertible_to<_Tp&, _Tp> && constructible_from<_Tp, const _Tp&> && convertible_to<const _Tp&, _Tp> && constructible_from<_Tp, const _Tp> && convertible_to<const _Tp, _Tp>;

template <typename _Tp>
concept swappable = requires(_Tp& __a, _Tp& __b) { swap(__a, __b); };

template <typename _Tp>
concept movable
    = is_object_v<
          _Tp> && move_constructible<_Tp> && assignable_from<_Tp&, _Tp> && swappable<_Tp>;

template <typename _Tp>
concept copyable
    = copy_constructible<
          _Tp> && movable<_Tp> && assignable_from<_Tp&, _Tp&> && assignable_from<_Tp&, const _Tp&> && assignable_from<_Tp&, const _Tp>;
}
#endif
#endif
