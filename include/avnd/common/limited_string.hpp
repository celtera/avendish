#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <string>
#include <algorithm>

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
    // size()+1 because of null terminator, guaranteed in std::string
    std::copy_n(data(), size() + 1, reinterpret_cast<char*>(dest));
  }
};

}
