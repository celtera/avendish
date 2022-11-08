#pragma once

#include <boost/pfr.hpp>
namespace boost::pfr
{
constexpr auto structure_to_typelist(const auto& s) noexcept
{
  return []<typename... Args>(boost::pfr::detail::sequence_tuple::tuple<Args&...>) {
    return avnd::typelist<std::decay_t<Args>...>{};
  }(boost::pfr::detail::tie_as_tuple(s));
}
}

namespace avnd
{
namespace pfr_simple
{
using namespace boost::pfr;

template <class T>
constexpr std::size_t fields_count_unsafe() noexcept
{
  using type = std::remove_cv_t<T>;

  constexpr std::size_t middle = sizeof(type) / 2 + 1;
  return boost::pfr::detail::detect_fields_count<T, 0, middle>(
      boost::pfr::detail::multi_element_range{}, 1L);
}

template <typename T>
using as_tuple_ref = decltype(boost::pfr::detail::tie_as_tuple(std::declval<T&>()));
template <typename T>
using as_tuple = decltype(boost::pfr::structure_to_tuple(std::declval<T&>()));
template <typename T>
using as_typelist = decltype(boost::pfr::structure_to_typelist(std::declval<T&>()));

// Yields a tuple with the compile-time function applied to each member of the struct
template <template <typename...> typename F, typename T>
using struct_apply = boost::mp11::mp_transform<F, as_tuple<T>>;
}
}
