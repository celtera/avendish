#pragma once
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/mp11/algorithm.hpp>
#include <boost/pfr.hpp>

#include <avnd/common/aggregates.recursive.hpp>
#include <avnd/common/aggregates.simple.hpp>

namespace avnd::pfr
{
template <typename T>
concept aggregate_argument = std::is_aggregate_v<std::decay_t<T>>;
namespace detail
{
template <class T>
  requires(aggregate_argument<T> && avnd::has_recursive_groups<T>)
constexpr AVND_INLINE auto tie_as_tuple(T& val)
{
  return pfr_recursive::detail::tie_as_tuple(val);
}
template <class T>
  requires(aggregate_argument<T> && !avnd::has_recursive_groups<T>)
constexpr AVND_INLINE auto tie_as_tuple(T& val)
{
  return pfr_simple::detail::tie_as_tuple(val);
}

}

template <typename T>
struct pfr_impl_helper;
template <typename T>
  requires(aggregate_argument<T> && avnd::has_recursive_groups<T>)
struct pfr_impl_helper<T>
{
  using as_typelist = pfr_recursive::as_typelist<T>;
  using tuple_size = pfr_recursive::tuple_size<T>;
  template <std::size_t N>
  using tuple_element_t = pfr_recursive::tuple_element_t<N, T>;
};
template <typename T>
  requires(aggregate_argument<T> && !avnd::has_recursive_groups<T>)
struct pfr_impl_helper<T>
{
  using as_typelist = pfr_simple::as_typelist<T>;
  using tuple_size = pfr_simple::tuple_size<T>;
  template <std::size_t N>
  using tuple_element_t = pfr_simple::tuple_element_t<N, T>;
};

// template <typename T>
// using tuple_size = typename pfr_impl_helper<T>::tuple_size;
template <typename T>
inline constexpr void* tuple_size_v{};

template <typename T>
  requires(aggregate_argument<std::decay_t<T>> && avnd::has_recursive_groups<T>)
inline constexpr size_t tuple_size_v<T> = pfr_recursive::tuple_size<T>::value;

template <typename T>
  requires(aggregate_argument<T> && !avnd::has_recursive_groups<T>)
inline constexpr size_t tuple_size_v<T> = pfr_simple::tuple_size<T>::value;

template <std::size_t N, typename T>
  requires(aggregate_argument<T> && avnd::has_recursive_groups<T>)
constexpr AVND_INLINE auto& get(T&& v)
{
  return pfr_recursive::get<N>(std::forward<T>(v));
}

template <std::size_t N, typename T>
  requires(aggregate_argument<T> && !avnd::has_recursive_groups<T>)
constexpr AVND_INLINE auto& get(T&& v)
{
  return pfr_simple::get<N>(std::forward<T>(v));
}

template <std::size_t N, typename T>
using tuple_element_t = typename pfr_impl_helper<T>::template tuple_element_t<N>;
}

namespace avnd
{

template <typename T>
using as_typelist = typename pfr::pfr_impl_helper<T>::as_typelist;

}
