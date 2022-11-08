#pragma once

#include <boost/mp11/algorithm.hpp>
#include <boost/pfr.hpp>
#include <avnd/concepts/modules.hpp>
namespace avnd
{
namespace pfr_recursive
{
namespace detail
{
template <class T>
constexpr AVND_INLINE auto tie_as_tuple(T& val)
{
  auto t = avnd::flatten_tuple(avnd::detail::detuple<T, true>(val));
  static_assert(flattening::is_tuple<decltype(t)>::value);
  return t;
}
}

constexpr AVND_INLINE auto structure_to_typelist(const auto& s) noexcept
{
  static_assert(flattening::is_tuple<decltype(detail::tie_as_tuple(s))>::value);
  // mp11 : flatten all the types with "recursive_module" defined in them.
  return []<template <typename...> typename T, typename... Args>(T<Args...>) {
    return avnd::typelist<std::decay_t<Args>...>{};
  }(detail::tie_as_tuple(s));
}

template <std::size_t N, typename T>
constexpr AVND_INLINE auto get(T&& v) -> decltype(auto)
{
  return get<N>(detail::tie_as_tuple(v));
}

template <std::size_t N, typename T>
using tuple_element_t = std::remove_reference_t<
    std::tuple_element_t<N, decltype(detail::tie_as_tuple(std::declval<T&>()))>>;

template <typename T>
using tuple_size = boost::mp11::mp_size<avnd::recursive_groups_transform_t<T>>;

template <typename T>
inline constexpr size_t tuple_size_v = tuple_size<T>::value;

template <class T>
constexpr AVND_INLINE std::size_t fields_count() noexcept
{
  return tuple_size_v<T>;
}

template <class T>
constexpr std::size_t fields_count_unsafe() noexcept
{
  return pfr_recursive::tuple_size_v<T>;
}
template <typename T>
using as_typelist = decltype(pfr_recursive::structure_to_typelist(std::declval<T&>()));
}
}
