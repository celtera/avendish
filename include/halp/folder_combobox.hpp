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
// needed. The VALUE is the selected file name (a string), so an object can use
// it directly as a file name. `folder_port` names the sibling path/folder
// port; `extensions` is a space-separated list of accepted suffixes (empty =
// all files); `init` is the initially selected file name — it stays selected
// even while the file does not exist yet, so chains of processes that write
// then read conventionally-named files keep working with default values.
template <
    static_string lit, static_string folder = "Folder", static_string exts = "",
    static_string init_value = "-">
struct folder_combobox
{
  enum widget
  {
    combobox
  };

  struct range
  {
    std::array<std::string_view, 1> values{init_value.value};
    std::string_view init{init_value.value};
  };

  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
  static clang_buggy_consteval auto folder_port() { return std::string_view{folder.value}; }
  static clang_buggy_consteval auto extensions() { return std::string_view{exts.value}; }

  std::string value{};

  operator std::string&() noexcept { return value; }
  operator const std::string&() const noexcept { return value; }
  auto& operator=(std::string t) noexcept
  {
    value = std::move(t);
    return *this;
  }
};
}
