#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/concepts/generic.hpp>
#include <boost/pfr.hpp>

namespace avnd
{
// type_or_value_qualification(modules)
// type_or_value_reflection(modules)

/**
 * recursive_group defines a group of multiple objects.
 *
 * Example usage:
 *
 * ```
 * struct EqualizerStrip {
 *   enum { recursive_group; };
 *   struct { float value; } gain;
 *   struct { float value; } bass;
 *   struct { float value; } treble;
 * }
 *
 * struct {
 *   EqualizerStrip strip1;
 *   EqualizerStrip strip2;
 *   EqualizerStrip strip3;
 * } inputs;
 * ```
 */

// Concepts and helper types
template <typename T>
concept recursive_group = requires
{
  T::recursive_group;
};

template <typename T>
using is_recursive_group = boost::mp11::mp_bool<recursive_group<T>>;

template <typename T>
struct recursive_groups_transform;

template <typename T>
using recursive_groups_transform_t = typename recursive_groups_transform<T>::type;

template <typename T>
struct recursive_groups_transform
{
  using tuple_type = decltype(boost::pfr::structure_to_tuple(T{}));
  using transformed_type = boost::mp11::mp_transform_if<
      avnd::is_recursive_group, recursive_groups_transform_t, tuple_type>;
  using type = boost::mp11::mp_flatten<transformed_type>;
};

// Flattening some recursive tuples BS

namespace flattening
{
// Simple traits
template <typename T>
struct is_tuple : std::false_type
{
};
template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type
{
};

// utility to ensure return type is a tuple
template <typename T>
constexpr decltype(auto) as_tuple(T& t)
{
  return std::tie(t);
}

template <typename... Ts>
constexpr decltype(auto) as_tuple(std::tuple<Ts...> t)
{
  return t;
}
template <typename... Ts>
constexpr decltype(auto) as_tuple(std::tuple<Ts...>& t)
{
  return t;
}

template <typename... Ts>
requires(is_tuple<Ts>::value || ...) constexpr decltype(auto)
    flatten(std::tuple<Ts...> t);
template <typename... Ts>
requires(!(is_tuple<Ts>::value || ...)) constexpr decltype(auto)
    flatten(std::tuple<Ts...> t);

// Simple case
template <typename T>
requires(!is_tuple<std::decay_t<T>>::value) constexpr T& flatten(T& t)
{
  return t;
}

// Possibly recursive tuple
template <typename T>
constexpr decltype(auto) flatten(std::tuple<T>& t)
{
  return flatten(std::get<0>(t));
}
template <typename T>
constexpr decltype(auto) flatten(std::tuple<T>&& t)
{
  return flatten(std::get<0>(t));
}
template <typename T>
constexpr decltype(auto) flatten(const std::tuple<T>& t)
{
  return flatten(std::get<0>(t));
}

// No more recursion, (sizeof...Ts != 1) with above overload
template <typename... Ts>
requires(!(is_tuple<Ts>::value || ...)) constexpr decltype(auto)
    flatten(std::tuple<Ts...> t)
{
  return t;
}

// Handle recursion
template <typename... Ts>
requires(is_tuple<Ts>::value || ...) constexpr decltype(auto)
    flatten(std::tuple<Ts...> t)
{
  return std::apply(
      []<typename... TS>(TS&&... ts) {
    return flatten(std::tuple_cat(as_tuple(flatten(std::forward<TS>(ts)))...));
      },
      t);
}
} // namespace flattening

template <typename... Ts>
constexpr auto flatten_tuple(std::tuple<Ts...>&& x)
{
  using flatten_t = decltype(flattening::flatten(x));
  if constexpr(!flattening::is_tuple<flatten_t>::value)
  {
    return std::tuple<flatten_t>{flattening::flatten(x)};
  }
  else
  {
    return flattening::flatten(x);
  }
}

namespace detail
{
template <typename T>
requires(!flattening::is_tuple<std::decay_t<T>>::value) constexpr auto& ref_or_copy(
    const T& t)
{
  static_assert(!flattening::is_tuple<std::decay_t<T>>::value);
  return t;
}
template <typename T>
requires(!flattening::is_tuple<std::decay_t<T>>::value) constexpr auto& ref_or_copy(T& t)
{
  static_assert(!flattening::is_tuple<std::decay_t<T>>::value);
  return t;
}
template <typename T>
requires(!flattening::is_tuple<std::decay_t<T>>::value) constexpr auto& ref_or_copy(
    T&& t)
{
  static_assert(!flattening::is_tuple<std::decay_t<T>>::value);
  return t;
}
template <typename T>
constexpr auto ref_or_copy(const std::tuple<T>& t)
{
  return t;
}
template <typename T>
constexpr auto ref_or_copy(std::tuple<T>& t)
{
  return t;
}
template <typename T>
constexpr auto ref_or_copy(std::tuple<T>&& t)
{
  return t;
}

template <typename... T>
constexpr auto ref_or_copy(const std::tuple<T...>& t)
{
  return t;
}
template <typename... T>
constexpr auto ref_or_copy(std::tuple<T...>& t)
{
  return t;
}
template <typename... T>
constexpr auto ref_or_copy(std::tuple<T...>&& t)
{
  return t;
}

static_assert(std::is_same_v<
              decltype(ref_or_copy(std::declval<std::tuple<int>&>())), std::tuple<int>>);

template <typename... _Elements>
constexpr auto ref_or_copy_as_tuple(_Elements&&... __args) noexcept
{
  return std::tuple<decltype(ref_or_copy<_Elements>(__args))...>{
      ref_or_copy<_Elements>(__args)...};
}

// static_assert(
//     std::is_same_v<
//         decltype(ref_or_copy_as_tuple(std::declval<int &>(),
//                                       std::declval<std::tuple<float &>
//                                       &>())),
//         std::tuple<std::tuple<int &>, std::tuple<float &>>>);
template <class T, bool rec = recursive_group<T>>
constexpr auto detuple(T& val) -> decltype(auto)
{
  if constexpr(!rec)
  {
    return val;
  }
  else
  {

    auto f = []<typename... Args>(Args&&... args) {
      return std::tuple_cat(ref_or_copy_as_tuple(detuple(args))...);
    };
    return std::apply(f, boost::pfr::structure_tie(val));
  }
}

} // namespace detail
}
