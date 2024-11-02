#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>

#include <algorithm>
#include <array>
#include <numeric>
#include <utility>

namespace avnd
{
template <std::size_t N>
struct num
{
  static constexpr const std::size_t value = N;
};

template <typename T>
struct typed_index_sequence;

template <typename T, T... Seq>
struct typed_index_sequence<std::integer_sequence<T, Seq...>>
{
  using type = boost::mp11::mp_list<boost::mp11::mp_int<Seq>...>;
};

template <typename T>
using typed_index_sequence_t = typename typed_index_sequence<T>::type;

template <typename T>
struct numbered_index_sequence;

template <typename T, T... Seq>
struct numbered_index_sequence<boost::mp11::mp_list<std::integral_constant<T, Seq>...>>
{
  using type = std::integer_sequence<T, Seq...>;
};

template <>
struct numbered_index_sequence<boost::mp11::mp_list<>>
{
  using type = std::integer_sequence<std::size_t>;
};

template <typename T>
using numbered_index_sequence_t = typename numbered_index_sequence<T>::type;

template <typename T, T... N>
inline constexpr auto integer_sequence_to_array(std::integer_sequence<T, N...>)
{
  return std::array<T, sizeof...(N)>{N...};
}

template <typename T, T... N>
inline constexpr auto integer_sequence_to_inverse_array(std::integer_sequence<T, N...>)
{
  if constexpr(sizeof...(N) > 0)
  {
    constexpr int max = (std::max)({N...});
    std::array<T, max + 1> ret{};
    int k = 0;
    for(T val : {N...})
    {
      ret[val] = k++;
    }
    return ret;
  }
  else
  {
    return std::array<T, 0>{};
  }
}

}
