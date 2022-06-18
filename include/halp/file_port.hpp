#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/inline.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <cstddef>
#include <string_view>
#include <vector>

namespace halp
{
// TODO look into using the LLFIO concepts instead for maximum power
struct file_view {
  std::string_view bytes;

  // std::fs::path would be great but limits to macOS 10.15+
  std::string_view filename;
};

template <halp::static_string lit>
struct file_port
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  HALP_INLINE_FLATTEN operator file_view&() noexcept { return file; }
  HALP_INLINE_FLATTEN operator const file_view&() const noexcept { return file; }
  //HALP_INLINE_FLATTEN operator bool() const noexcept { return soundfile.data && soundfile.channels > 0 && soundfile.frames > 0; }

  file_view file;
};
}

#include <avnd/concepts/file_port.hpp>
static_assert(avnd::raw_file_port<halp::file_port<"foo">>);
