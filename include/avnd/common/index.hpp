#pragma once
#include <avnd/common/tuple.hpp>

#include <array>
#include <utility>

namespace avnd
{
template <std::size_t N>
struct field_index
{
  consteval operator std::size_t() const noexcept { return N; }
};

template <std::size_t N>
struct predicate_index
{
  consteval operator std::size_t() const noexcept { return N; }
};

template <std::size_t Idx, typename Field>
struct field_reflection
{
  using type = Field;
  static const constexpr auto index = Idx;
};

template <std::size_t... I>
static constexpr auto field_indices(std::index_sequence<I...>)
{
  return tpl::tuple<avnd::field_index<I>...>{};
}

template <int N, typename T, std::size_t M>
constexpr int index_of_element(const std::array<T, M>& arr) noexcept
{
  static_assert(M > 0);

  int ret = [&arr] {
    int k = 0;
    for(int i : arr)
    {
      if(i == N)
        return k;
      k++;
    }
    return -1;
  }();

  return ret;
}

template <typename T, std::size_t M>
constexpr int index_of_element(T N, const std::array<T, M>& arr) noexcept
{
  static_assert(M > 0);

  int ret = [N, &arr] {
    int k = 0;
    for(int i : arr)
    {
      if(i == N)
        return k;
      k++;
    }
    return -1;
  }();

  return ret;
}

template <int N, typename T, T... Idx>
consteval int index_of_element(std::integer_sequence<T, Idx...>) noexcept
{
  static_assert(sizeof...(Idx) > 0);

  constexpr int ret = [] {
    int k = 0;
    for(int i : {Idx...})
    {
      if(i == N)
        return k;
      k++;
    }
    return -1;
  }();

  static_assert(ret >= 0);
  return ret;
}

template <typename T, T... Idx>
constexpr int index_of_element(T N, std::integer_sequence<T, Idx...>) noexcept
{
  static_assert(sizeof...(Idx) > 0);

  int ret = [N] {
    int k = 0;
    for(int i : {Idx...})
    {
      if(i == N)
        return k;
      k++;
    }
    return -1;
  }();

  return ret;
}
}
