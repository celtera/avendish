#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/inline.hpp>
#include <avnd/common/tuple.hpp>
#include <avnd/concepts/generic.hpp>
#include <boost/pfr.hpp>

namespace boost::pfr
{
// Some tuple utilities as std::tuple is *slow*
namespace detail
{

template <class T, std::size_t... I>
constexpr AVND_INLINE auto
make_tpltuple_from_tietuple(const T& t, std::index_sequence<I...>) noexcept
{
  return tpl::make_tuple(boost::pfr::detail::sequence_tuple::get<I>(t)...);
}

template <class T, std::size_t... I>
constexpr AVND_INLINE auto
make_tpltiedtuple_from_tietuple(const T& t, std::index_sequence<I...>) noexcept
{
  return tpl::tie(boost::pfr::detail::sequence_tuple::get<I>(t)...);
}

template <class T, std::size_t... I>
constexpr AVND_INLINE auto
make_consttpltiedtuple_from_tietuple(const T& t, std::index_sequence<I...>) noexcept
{
  return tpl::tuple<std::add_lvalue_reference_t<std::add_const_t<std::remove_reference_t<
      decltype(boost::pfr::detail::sequence_tuple::get<I>(t))>>>...>(
      boost::pfr::detail::sequence_tuple::get<I>(t)...);
}

}
}
namespace avnd
{

template <class T>
constexpr AVND_INLINE auto structure_tpltie(const T& val) noexcept
{
  return boost::pfr::detail::make_consttpltiedtuple_from_tietuple(
      boost::pfr::detail::tie_as_tuple(const_cast<T&>(val)),
      boost::pfr::detail::make_index_sequence<boost::pfr::tuple_size_v<T>>());
}

template <class T>
constexpr AVND_INLINE auto structure_tpltie(T& val) noexcept
{
  return boost::pfr::detail::make_tpltiedtuple_from_tietuple(
      boost::pfr::detail::tie_as_tuple(val),
      boost::pfr::detail::make_index_sequence<boost::pfr::tuple_size_v<T>>());
}

template <class T>
constexpr AVND_INLINE auto structure_to_tpltuple(const T& val) noexcept
{
  return boost::pfr::detail::make_tpltuple_from_tietuple(
      boost::pfr::detail::tie_as_tuple(val),
      boost::pfr::detail::make_index_sequence<boost::pfr::tuple_size_v<T>>());
}

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
concept recursive_group = requires { T::recursive_group; };

template <typename T>
using is_recursive_group = boost::mp11::mp_bool<recursive_group<T>>;

template <typename T>
struct recursive_groups_transform;

template <typename T>
using recursive_groups_transform_t = typename recursive_groups_transform<T>::type;

template <typename T>
struct recursive_groups_transform
{
  using tuple_type
      = decltype(avnd::structure_to_tpltuple(std::declval<std::decay_t<T>>()));
  using transformed_type = boost::mp11::mp_transform_if<
      avnd::is_recursive_group, recursive_groups_transform_t, tuple_type>;
  using type = boost::mp11::mp_flatten<transformed_type>;
};

// Check if any member is a recursive group - if not we can generally use the simple PFR implementation.
template <typename T>
struct has_recursive_groups_impl
{
  static_assert(std::is_aggregate_v<T>);
  static constexpr bool value = boost::mp11::mp_any_of<
      decltype(avnd::structure_to_tpltuple(std::declval<std::decay_t<T>>())),
      is_recursive_group>::value;
};

template <typename T>
concept has_recursive_groups
    = has_recursive_groups_impl<std::remove_reference_t<std::remove_const_t<T>>>::value;

// Flattening some recursive tuples BS

namespace flattening
{
// Simple traits
template <typename T>
struct is_tuple : std::false_type
{
};
template <typename... Ts>
struct is_tuple<tpl::tuple<Ts...>> : std::true_type
{
};

// utility to ensure return type is a tuple
template <typename T>
constexpr AVND_INLINE decltype(auto) as_tuple(T& t)
{
  return tpl::tie(t);
}

template <typename... Ts>
constexpr AVND_INLINE decltype(auto) as_tuple(tpl::tuple<Ts...> t)
{
  return t;
}
template <typename... Ts>
constexpr AVND_INLINE decltype(auto) as_tuple(tpl::tuple<Ts...>& t)
{
  return t;
}

template <typename... Ts>
  requires(is_tuple<Ts>::value || ...)
constexpr decltype(auto) flatten(tpl::tuple<Ts...> t);
template <typename... Ts>
  requires(!(is_tuple<Ts>::value || ...))
constexpr decltype(auto) flatten(tpl::tuple<Ts...> t);

// Simple case
template <typename T>
  requires(!is_tuple<std::decay_t<T>>::value)
constexpr AVND_INLINE T& flatten(T& t)
{
  return t;
}

// Possibly recursive tuple
template <typename T>
constexpr AVND_INLINE decltype(auto) flatten(tpl::tuple<T>& t)
{
  return flatten(get<0>(t));
}
template <typename T>
constexpr AVND_INLINE decltype(auto) flatten(tpl::tuple<T>&& t)
{
  return flatten(get<0>(t));
}
template <typename T>
constexpr AVND_INLINE decltype(auto) flatten(const tpl::tuple<T>& t)
{
  return flatten(get<0>(t));
}

// No more recursion, (sizeof...Ts != 1) with above overload
template <typename... Ts>
  requires(!(is_tuple<Ts>::value || ...))
constexpr AVND_INLINE decltype(auto) flatten(tpl::tuple<Ts...> t)
{
  return t;
}

// Handle recursion
template <typename... Ts>
  requires(is_tuple<Ts>::value || ...)
constexpr AVND_INLINE decltype(auto) flatten(tpl::tuple<Ts...> t)
{
  return tpl::apply(
      []<typename... TS>(TS&&... ts) {
    return flatten(tpl::tuple_cat(as_tuple(flatten(std::forward<TS>(ts)))...));
      },
      t);
}
} // namespace flattening

template <typename... Ts>
constexpr AVND_INLINE auto flatten_tuple(tpl::tuple<Ts...>&& x)
{
  using flatten_t = decltype(flattening::flatten(x));
  if constexpr(!flattening::is_tuple<flatten_t>::value)
  {
    return tpl::tuple<flatten_t>{flattening::flatten(x)};
  }
  else
  {
    return flattening::flatten(x);
  }
}

namespace detail
{
template <typename T>
  requires(!flattening::is_tuple<std::decay_t<T>>::value)
constexpr AVND_INLINE auto& ref_or_copy(const T& t)
{
  static_assert(!flattening::is_tuple<std::decay_t<T>>::value);
  return t;
}
template <typename T>
  requires(!flattening::is_tuple<std::decay_t<T>>::value)
constexpr AVND_INLINE auto& ref_or_copy(T& t)
{
  static_assert(!flattening::is_tuple<std::decay_t<T>>::value);
  return t;
}
template <typename T>
  requires(!flattening::is_tuple<std::decay_t<T>>::value)
constexpr AVND_INLINE auto& ref_or_copy(T&& t)
{
  static_assert(!flattening::is_tuple<std::decay_t<T>>::value);
  return t;
}
template <typename T>
constexpr AVND_INLINE auto ref_or_copy(const tpl::tuple<T>& t)
{
  return t;
}
template <typename T>
constexpr AVND_INLINE auto ref_or_copy(tpl::tuple<T>& t)
{
  return t;
}
template <typename T>
constexpr AVND_INLINE auto ref_or_copy(tpl::tuple<T>&& t)
{
  return t;
}

template <typename... T>
constexpr AVND_INLINE auto ref_or_copy(const tpl::tuple<T...>& t)
{
  return t;
}
template <typename... T>
constexpr AVND_INLINE auto ref_or_copy(tpl::tuple<T...>& t)
{
  return t;
}
template <typename... T>
constexpr AVND_INLINE auto ref_or_copy(tpl::tuple<T...>&& t)
{
  return t;
}

static_assert(std::is_same_v<
              decltype(ref_or_copy(std::declval<tpl::tuple<int>&>())), tpl::tuple<int>>);

template <typename... _Elements>
constexpr AVND_INLINE auto ref_or_copy_as_tuple(_Elements&&... __args) noexcept
{
  return tpl::tuple<decltype(ref_or_copy<_Elements>(__args))...>{
      ref_or_copy<_Elements>(__args)...};
}

template <class T, bool rec = recursive_group<T>>
constexpr AVND_INLINE auto detuple(T& val) -> decltype(auto)
{
  if constexpr(!rec)
  {
    return val;
  }
  else
  {
    auto f = []<typename... Args>(Args&&... args) {
      return tpl::tuple_cat(ref_or_copy_as_tuple(detuple(args))...);
    };
    return tpl::apply(f, avnd::structure_tpltie(val));
  }
}

} // namespace detail
}
