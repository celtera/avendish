#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace halp
{
// Combobox whose items are set at runtime via update_items. Bindings without
// support leave update_items empty and use the static range() fallback.
template <static_string lit>
struct dynamic_combobox
{
  enum widget
  {
    combobox
  };

  struct range
  {
    std::array<std::string_view, 1> values{"-"};
    int init{0};
  };

  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  int value{0};

  std::function<void(std::vector<std::string>)> update_items;

  operator int&() noexcept { return value; }
  operator const int&() const noexcept { return value; }
  auto& operator=(int t) noexcept
  {
    value = t;
    return *this;
  }
};
}
