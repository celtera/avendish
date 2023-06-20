#pragma once
#include <halp/inline.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <cinttypes>
#include <cstdint>
#include <span>
#include <string_view>

namespace halp
{
template <typename T>
struct soundfile_view
{
  const T** data{};
  int64_t frames{};
  int32_t channels{};
  int32_t rate{};

  // std::fs::path would be great but limits to macOS 10.15+
  std::string_view filename;
};

template <halp::static_string lit, typename T = float>
struct soundfile_port
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
  static clang_buggy_consteval auto filters()
  {
    enum
    {
      audio
    };
    return audio;
  }

  HALP_INLINE_FLATTEN operator soundfile_view<T>&() noexcept { return soundfile; }

  HALP_INLINE_FLATTEN operator const soundfile_view<T>&() const noexcept
  {
    return soundfile;
  }
  HALP_INLINE_FLATTEN operator bool() const noexcept
  {
    return soundfile.data && soundfile.channels > 0 && soundfile.frames > 0;
  }

  HALP_INLINE_FLATTEN std::span<const T> channel(int channel) const noexcept
  {
    return std::span(soundfile.data[channel], soundfile.frames);
  }

  [[nodiscard]] HALP_INLINE_FLATTEN int channels() const noexcept
  {
    return soundfile.channels;
  }

  [[nodiscard]] HALP_INLINE_FLATTEN int64_t frames() const noexcept
  {
    return soundfile.frames;
  }

  HALP_INLINE_FLATTEN const T* operator[](int channel) const noexcept
  {
    return soundfile.data[channel];
  }

  soundfile_view<T> soundfile;
};
}
