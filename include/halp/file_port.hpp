#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/inline.hpp>
#include <halp/modules.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

HALP_MODULE_EXPORT
namespace halp
{
// TODO look into using the LLFIO concepts instead for maximum power
struct binary_file_view
{
  std::string_view bytes;

  // std::fs::path would be great but limits to macOS 10.15+
  std::string_view filename;
};

struct text_file_view
{
  std::string_view bytes;

  // std::fs::path would be great but limits to macOS 10.15+
  std::string_view filename;

  enum
  {
    text
  };
};
struct mmap_file_view
{
  std::string_view bytes;

  // std::fs::path would be great but limits to macOS 10.15+
  std::string_view filename;

  enum
  {
    mmap
  };
};

struct output_file_view
{
  std::string_view filename;
  enum
  {
    file_create
  };
};

template <halp::static_string lit, typename FileType = text_file_view>
struct file_port
{
  using file_type = FileType;
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  HALP_INLINE_FLATTEN operator FileType&() noexcept { return file; }
  HALP_INLINE_FLATTEN operator const FileType&() const noexcept { return file; }
  HALP_INLINE_FLATTEN operator bool() const noexcept { return !file.bytes.empty(); }

  FileType file;
};

//! The path of a file, picked with a file dialog: the object gets the path,
//! not the file's contents (for that, see file_port). `filters` is a
//! file-dialog filter such as "Text files (*.txt *.csv)"; empty lists all.
template <halp::static_string lit, halp::static_string filters_lit = "">
struct file_path
{
  enum widget { file };
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
  static clang_buggy_consteval auto filters()
  {
    return std::string_view{filters_lit.value};
  }

  HALP_INLINE_FLATTEN operator std::string_view() noexcept { return value; }
  HALP_INLINE_FLATTEN operator bool() const noexcept { return !value.empty(); }
  auto& operator=(const std::string& t)
  {
    value = t;
    return *this;
  }
  auto& operator=(std::string&& t) noexcept
  {
    value = std::move(t);
    return *this;
  }

  std::string value;
};

//! A file path to write to, typed (a file dialog could only pick existing
//! files): the host may expand placeholders in it, e.g. %t (the date and time)
//! and %n (a number making the name unique).
template <halp::static_string lit>
struct save_file_path : halp::lineedit_t<lit, "">
{
  using halp::lineedit_t<lit, "">::operator=;
  static constexpr bool placeholders() { return true; }
};

template <halp::static_string lit>
struct file_write_port
{
  using file_type = output_file_view;
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  HALP_INLINE_FLATTEN operator output_file_view&() noexcept { return file; }
  HALP_INLINE_FLATTEN operator const output_file_view&() const noexcept { return file; }

  output_file_view file;
};

struct folder_port
{
  enum widget { folder };
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  HALP_INLINE_FLATTEN operator std::string_view() noexcept { return value; }
  HALP_INLINE_FLATTEN operator bool() const noexcept { return !value.empty(); }

  std::string value;
};
}
