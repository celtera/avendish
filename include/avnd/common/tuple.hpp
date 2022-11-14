#pragma once

#if __has_include(<tuplet/tuple.hpp>)
#include <tuplet/tuple.hpp>
namespace tpl = tuplet;
#else
#include <tuple>
namespace tpl = std;
#endif

namespace avnd
{
#if defined(__clang__)
template <std::size_t N, typename T>
struct tuple_element_impl;
template <std::size_t N, typename... T>
struct tuple_element_impl<N, tpl::tuple<T...>>
{
  using type = __type_pack_element<N, T...>;
};
#else
template <std::size_t N, typename T>
using tuple_element_impl = std::tuple_element<N, T>;
#endif
}
