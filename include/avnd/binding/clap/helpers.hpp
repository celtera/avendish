#pragma once
#include <algorithm>
#include <string_view>

namespace avnd_clap
{
template <std::size_t M>
auto copy_string(char (&field)[M], std::string_view text)
{
  auto chars = std::min(text.size(), M - 1);
  std::copy_n(text.data(), chars, field);
  field[chars] = 0;
}
}
