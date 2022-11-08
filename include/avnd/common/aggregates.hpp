#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/index_sequence.hpp>

#include <string_view>
#include <tuple>

namespace avnd
{
template <typename...>
struct typelist
{
};
}

#if !defined(AVND_USE_BOOST_PFR)
#define AVND_USE_BOOST_PFR 1
#endif

#if AVND_USE_BOOST_PFR
#include <boost/mp11/algorithm.hpp>
#include <boost/pfr.hpp>

#include <avnd/common/aggregates.recursive.hpp>
#include <avnd/common/aggregates.simple.hpp>

namespace avnd::pfr
{

namespace detail
{
template <class T>
requires(avnd::has_recursive_groups<T>) constexpr AVND_INLINE auto tie_as_tuple(T& val)
{
  return pfr_recursive::detail::tie_as_tuple(val);
}
template <class T>
requires(!avnd::has_recursive_groups<T>) constexpr AVND_INLINE auto tie_as_tuple(T& val)
{
  return pfr_simple::detail::tie_as_tuple(val);
}

}

template <typename T>
struct pfr_impl_helper;
template <typename T>
requires(avnd::has_recursive_groups<T>) struct pfr_impl_helper<T>
{
  using as_typelist = pfr_recursive::as_typelist<T>;
  using tuple_size = pfr_recursive::tuple_size<T>;
  template <std::size_t N>
  using tuple_element_t = pfr_recursive::tuple_element_t<N, T>;
};
template <typename T>
requires(!avnd::has_recursive_groups<T>) struct pfr_impl_helper<T>
{
  using as_typelist = pfr_simple::as_typelist<T>;
  using tuple_size = pfr_simple::tuple_size<T>;
  template <std::size_t N>
  using tuple_element_t = pfr_simple::tuple_element_t<N, T>;
};

template <typename T>
using tuple_size = typename pfr_impl_helper<T>::tuple_size;

template <typename T>
inline constexpr size_t tuple_size_v = tuple_size<T>::value;

template <typename T>
requires(avnd::has_recursive_groups<T>) constexpr AVND_INLINE
    auto structure_to_typelist(const T& s) noexcept
{
  return pfr_recursive::structure_to_typelist(s);
}

template <typename T>
requires(!avnd::has_recursive_groups<T>) constexpr AVND_INLINE
    auto structure_to_typelist(const T& s) noexcept
{
  return pfr_simple::structure_to_typelist(s);
}

template <std::size_t N, typename T>
requires(avnd::has_recursive_groups<T>) constexpr AVND_INLINE auto get(T&& v)
    -> decltype(auto)
{
  return pfr_recursive::get<N>(std::forward<T>(v));
}

template <std::size_t N, typename T>
requires(!avnd::has_recursive_groups<T>) constexpr AVND_INLINE auto get(T&& v)
    -> decltype(auto)
{
  return pfr_simple::get<N>(std::forward<T>(v));
}

template <class T>
constexpr AVND_INLINE std::size_t fields_count() noexcept
{
  return tuple_size_v<T>;
}

template <std::size_t N, typename T>
using tuple_element_t = typename pfr_impl_helper<T>::template tuple_element_t<N>;
}

namespace avnd
{

template <typename T>
using as_typelist = typename pfr::pfr_impl_helper<T>::as_typelist;

template <class T>
requires(avnd::has_recursive_groups<T>) constexpr std::size_t
    fields_count_unsafe() noexcept
{
  return pfr_recursive::fields_count_unsafe<T>();
}

template <class T>
requires(!avnd::has_recursive_groups<T>) constexpr std::size_t
    fields_count_unsafe() noexcept
{
  return pfr_simple::fields_count_unsafe<T>();
}

}
#else
#include <avnd/common/aggregates.p1061.hpp>
#endif
