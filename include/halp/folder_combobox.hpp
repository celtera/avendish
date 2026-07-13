#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <array>
#include <string>
#include <string_view>

namespace halp
{
// Combobox whose items are the files of a sibling folder port, listed by the
// host at edit time (and refreshed when the folder changes) — no execution
// needed. `folder_port` names the sibling path/folder port; `extensions` is a
// space-separated list of accepted suffixes (empty = all files).
template <static_string lit, static_string folder = "Folder", static_string exts = "">
struct folder_combobox
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
  static clang_buggy_consteval auto folder_port() { return std::string_view{folder.value}; }
  static clang_buggy_consteval auto extensions() { return std::string_view{exts.value}; }

  int value{0};

  operator int&() noexcept { return value; }
  operator const int&() const noexcept { return value; }
  auto& operator=(int t) noexcept
  {
    value = t;
    return *this;
  }
};
}
