#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cstddef>

namespace halp
{

template <std::size_t N>
struct static_string
{
  consteval static_string(const char (&str)[N]) noexcept
  {
    for (int i = 0; i < N; i++)
    {
      value[i] = str[i];
    }
  }

  char value[N];
};

template <std::size_t N, std::size_t M>
consteval bool operator==(static_string<N> lhs, static_string<M> rhs) noexcept
{
  if constexpr (M != N)
    return false;
  for (int i = 0; i < M; i++)
    if (lhs.value[i] != rhs.value[i])
      return false;
  return true;
}

template <std::size_t N, std::size_t M>
consteval bool operator!=(static_string<N> lhs, static_string<M> rhs) noexcept
{
  if constexpr (M != N)
    return true;
  for (int i = 0; i < M; i++)
    if (lhs.value[i] != rhs.value[i])
      return true;
  return false;
}
}
