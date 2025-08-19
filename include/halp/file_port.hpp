#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/inline.hpp>
#include <halp/modules.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <cstddef>
#include <string_view>
#include <vector>

HALP_MODULE_EXPORT
namespace halp
{
// TODO look into using the LLFIO concepts instead for maximum power
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

template <halp::static_string lit>
struct file_write_port
{
  using file_type = output_file_view;
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  HALP_INLINE_FLATTEN operator output_file_view&() noexcept { return file; }
  HALP_INLINE_FLATTEN operator const output_file_view&() const noexcept { return file; }

  output_file_view file;
};
}
