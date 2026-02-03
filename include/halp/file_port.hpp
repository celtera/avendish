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
struct folder_port
{
  enum widget { folder };
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  HALP_INLINE_FLATTEN operator std::string_view() noexcept { return value; }
  HALP_INLINE_FLATTEN operator bool() const noexcept { return !value.empty(); }

  std::string value;
};
}
