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

template <typename T>
using as_tuple = decltype(boost::pfr::structure_to_tuple(std::declval<T&>()));
template <typename T>
using as_typelist = decltype(boost::pfr::structure_to_typelist(std::declval<T&>()));
}
}
