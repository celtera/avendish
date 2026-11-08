#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

// The floating-point std::from_chars is missing from some standard libraries
// (older Apple deployment targets, FreeBSD's libc++)
#if defined(__cpp_lib_to_chars)
#include <charconv>
#else
#include <boost/lexical_cast/try_lexical_convert.hpp>
#endif

namespace halp
{
//! A folder combobox's value as an index, when it is a number: one that is
//! not, below 0 or not a number, is a file name. Past the largest int (or
//! infinite): the largest int.
[[nodiscard]] inline std::optional<int> folder_index(std::string_view value) noexcept
{
  double d{};
#if defined(__cpp_lib_to_chars)
  const auto end = value.data() + value.size();
  const auto [ptr, ec] = std::from_chars(value.data(), end, d);
  if(ec != std::errc{} || ptr != end)
    return std::nullopt;
#else
  // lexical_cast also takes a leading '+', which from_chars does not
  if(value.starts_with('+') || !boost::conversion::try_lexical_convert(value, d))
    return std::nullopt;
#endif
  if(!(d >= 0.))
    return std::nullopt;
  return d < double(std::numeric_limits<int>::max()) ? int(d)
                                                     : std::numeric_limits<int>::max();
}

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

  //! A number rather than a file name, e.g. sent by automation or a cable:
  //! the index of a file in the folder's list. File names have an extension,
  //! so a name is never taken for a number.
  [[nodiscard]] std::optional<int> index() const noexcept { return folder_index(value); }

  operator std::string&() noexcept { return value; }
  operator const std::string&() const noexcept { return value; }
  auto& operator=(std::string t) noexcept
  {
    value = std::move(t);
    return *this;
  }
};
}
