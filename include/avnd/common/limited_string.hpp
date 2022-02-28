#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <string>

namespace avnd
{

template <std::size_t N>
struct limited_string : std::string
{
  using std::string::string;
  constexpr limited_string(std::string&& str) noexcept
      : std::string{std::move(str)}
  {
    if (this->size() > N)
      this->resize(N);
  }

  void copy_to(void* dest) const noexcept
  {
    const auto src = data();
    const int n = size() + 1;
    const auto dst = reinterpret_cast<char*>(dest);
    for (int i = 0; i < n; i++)
    {
      dst[i] = src[i];
    }
  }
};

}
