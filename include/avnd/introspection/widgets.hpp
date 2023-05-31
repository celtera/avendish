#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/introspection/mapper.hpp>
#include <avnd/introspection/range.hpp>
#include <avnd/introspection/smooth.hpp>

#include <array>
#include <string_view>
#include <type_traits>

namespace avnd
{

// Utilities for mapping enums

template <std::size_t N>
static constexpr std::array<std::string_view, N>
to_string_view_array(const char* const (&a)[N])
{
  return [&]<std::size_t... I>(std::index_sequence<I...>)
      ->std::array<std::string_view, N>
  {
    return {{a[I]...}};
  }
  (std::make_index_sequence<N>{});
}

template <std::size_t N>
static constexpr std::array<std::string_view, N>
to_string_view_array(const std::string_view (&a)[N])
{
  return [&]<std::size_t... I>(std::index_sequence<I...>)
      ->std::array<std::string_view, N>
  {
    return {{a[I]...}};
  }
  (std::make_index_sequence<N>{});
}

template <std::size_t N>
static constexpr std::array<std::string_view, N>
to_string_view_array(const std::array<const char*, N>& a)
{
  return [&]<std::size_t... I>(std::index_sequence<I...>)
      ->std::array<std::string_view, N>
  {
    return {{a[I]...}};
  }
  (std::make_index_sequence<N>{});
}

template <typename T, std::size_t N>
static constexpr std::array<std::string_view, N>
to_string_view_array(const std::pair<std::string_view, T> (&a)[N])
{
  return [&]<std::size_t... I>(std::index_sequence<I...>)
      ->std::array<std::string_view, N>
  {
    return {{a[I].first...}};
  }
  (std::make_index_sequence<N>{});
}

template <std::size_t N>
static constexpr std::array<std::string_view, N>
to_string_view_array(const std::array<std::string_view, N>& a)
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
}
