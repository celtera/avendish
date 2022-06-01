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

/// Mapper reflection ////
/**
 * Used to define how UI sliders behave.
 */
template<typename T>
concept mapper = requires (T t)
{
  // From linear to eased domain
  { t.map(0.) } -> std::convertible_to<double>;
  // From eased domain to linear domain
  { t.unmap(0.) } -> std::convertible_to<double>;
};

template <typename C>
concept has_mapper =
  requires { C::mapper(); }
  || requires { sizeof(C::mapper); }
  || requires { sizeof(typename C::mapper); };

// Used to define how UI sliders behave.
template <avnd::has_mapper T>
consteval auto get_mapper()
{
  if constexpr (requires { sizeof(typename T::mapper); })
  {
    static_assert(mapper<typename T::mapper>);
    return typename T::mapper{};
  }
  else if constexpr (requires { T::mapper(); })
  {
    static_assert(mapper<std::decay_t<decltype(T::mapper())>>);
    return T::mapper();
  }
  else if constexpr (requires { sizeof(decltype(T::mapper)); })
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
