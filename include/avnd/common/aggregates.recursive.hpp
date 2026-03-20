#pragma once

#include <avnd/concepts/modules.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/pfr.hpp>
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

// Necessary because of
// https://github.com/llvm/llvm-project/issues/138018
struct structure_to_typelist_helper
{
  template <template <typename...> typename T, typename... Args>
  constexpr AVND_INLINE auto operator()(T<Args...>)
  {
    return avnd::typelist<std::decay_t<Args>...>{};
  }
};

constexpr AVND_INLINE auto structure_to_typelist(const auto& s) noexcept
{
  static_assert(flattening::is_tuple<decltype(detail::tie_as_tuple(s))>::value);
  // mp11 : flatten all the types with "recursive_module" defined in them.
  return structure_to_typelist_helper{}(detail::tie_as_tuple(s));
}

template <std::size_t N, typename T>
constexpr AVND_INLINE auto get(T&& v) -> decltype(auto)
{
  return get<N>(detail::tie_as_tuple(v));
}

template <std::size_t N, typename T>
using tuple_element_t = std::remove_reference_t<typename tuple_element_impl<
    N, decltype(detail::tie_as_tuple(std::declval<T&>()))>::type>;

template <typename T>
using tuple_size = boost::mp11::mp_size<avnd::recursive_groups_transform_t<T>>;

template <typename T>
inline constexpr size_t tuple_size_v = tuple_size<T>::value;

// manual inlining of structure_to_typelist
template <typename T>
using as_typelist = decltype(structure_to_typelist_helper{}(
    avnd::flatten_tuple(avnd::detail::detuple<T, true>(std::declval<T&>()))));
}
}
