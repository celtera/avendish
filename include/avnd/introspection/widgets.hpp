#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <type_traits>
#include <array>
#include <string_view>

namespace avnd
{
/// Widget reflection ///
template <typename C>
concept has_widget = requires
{
  std::is_enum_v<typename C::widget>;
};

/// Range reflection ///
template <typename C>
concept has_range =
  requires { C::range(); }
  || requires { sizeof(C::range); }
  || requires { sizeof(typename C::range); };

template <avnd::has_range T>
consteval auto get_range()
{
  if constexpr (requires { sizeof(typename T::range); })
    return typename T::range{};
  else if constexpr (requires { T::range(); })
    return T::range();
  else if constexpr (requires { sizeof(decltype(T::range)); })
    return T::range;
  else
    return T::there_is_no_range_here;
}

template <typename T>
consteval auto get_range(const T&)
{
  return get_range<T>();
}

template <std::size_t N>
static constexpr std::array<std::string_view, N> to_string_view_array(const char* const (&a)[N])
{
  return [&] <std::size_t... I> (std::index_sequence<I...>) -> std::array<std::string_view, N> {
      return { {a[I]...} };
  }(std::make_index_sequence<N>{});
}

template <std::size_t N>
static constexpr std::array<std::string_view, N> to_string_view_array(const std::string_view (&a)[N])
{
  return [&] <std::size_t... I> (std::index_sequence<I...>) -> std::array<std::string_view, N> {
    return { {a[I]...} };
  }(std::make_index_sequence<N>{});
}

template <std::size_t N>
static constexpr std::array<std::string_view, N> to_string_view_array(const std::array<const char*, N>& a)
{
  return [&] <std::size_t... I> (std::index_sequence<I...>) -> std::array<std::string_view, N> {
      return { {a[I]...} };
  }(std::make_index_sequence<N>{});
}

template <typename T, std::size_t N>
static constexpr std::array<std::string_view, N> to_string_view_array(const std::pair<std::string_view,T> (&a)[N])
{
  return [&] <std::size_t... I> (std::index_sequence<I...>) -> std::array<std::string_view, N> {
      return { {a[I].first...} };
  }(std::make_index_sequence<N>{});
}

template <std::size_t N>
static constexpr std::array<std::string_view, N> to_string_view_array(const std::array<std::string_view, N>& a)
{
  return a;
}


template <typename T>
consteval int get_enum_choices_count()
{
  if constexpr(requires { std::size(get_range<T>().values()); })
  {
    return std::ssize(get_range<T>().values());
  }
  else if constexpr(requires { std::size(get_range<T>().values); })
  {
    return std::ssize(get_range<T>().values);
  }
  else
  {
    return 0;
  }
}

template <typename T>
consteval auto get_enum_choices()
{
  if constexpr(requires { std::size(get_range<T>().values()); })
  {
    return to_string_view_array(get_range<T>().values());
  }
  else if constexpr(requires { std::size(get_range<T>().values); })
  {
    return to_string_view_array(get_range<T>().values);
  }
  else
  {
    return std::array<std::string_view, 0>{};
  }
}

/// Normalization reflection ////
template <typename C>
concept has_normalize =
  requires { C::normalize(); }
  || requires { sizeof(C::normalize); }
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
