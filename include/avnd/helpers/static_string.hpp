#pragma once
#include <cstddef>

namespace avnd
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

}
