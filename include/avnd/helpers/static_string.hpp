#pragma once
#include <algorithm>

namespace avnd
{

template <std::size_t N>
struct static_string
{
  consteval static_string(const char (&str)[N]) noexcept
  {
    std::copy_n(str, N, value);
  }

  char value[N];
};

}
