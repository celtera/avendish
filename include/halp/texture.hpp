#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>
#include <halp/texture_formats.hpp>
#include <boost/container/vector.hpp>

namespace halp
{

template <static_string lit>
struct texture_input
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  rgba_color get(int x, int y) noexcept
  {
    assert(x >= 0 && x < texture.width);
    assert(y >= 0 && y < texture.height);

    const int pixel_index = y * texture.width + x;
    const int byte_index = pixel_index * 4;

    auto* pixel_ptr = texture.bytes + byte_index;
    return {.r = pixel_ptr[0], .g = pixel_ptr[1], .b = pixel_ptr[2], .a = pixel_ptr[3]};
  }

  rgba_texture texture;
};

template <static_string lit>
struct rgb_texture_input
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  rgb_color get(int x, int y) noexcept
  {
    assert(x >= 0 && x < texture.width);
    assert(y >= 0 && y < texture.height);

    const int pixel_index = y * texture.width + x;
    const int byte_index = pixel_index * 3;

    auto* pixel_ptr = texture.bytes + byte_index;
    return {.r = pixel_ptr[0], .g = pixel_ptr[1], .b = pixel_ptr[2]};
  }

  rgb_texture texture;
};

template <static_string lit, typename TextureType = rgba_texture>
struct texture_output
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  constexpr texture_output() noexcept
  {
    texture.width = 0;
    texture.height = 0;
    texture.bytes = nullptr;
    texture.changed = true;
  }

  void create(int width, int height)
  {
    storage.resize(width * height * 4, boost::container::default_init);
    texture.width = width;
    texture.height = height;
    texture.changed = false;
    texture.bytes = storage.data();
  }

  void upload() noexcept { texture.changed = true; }

  void set(int x, int y, int r, int g, int b, int a = 255) noexcept
  {
    texture.set(x, y, r, g, b, a);
  }

  void set(int x, int y, rgba_color col) noexcept
  {
    texture.set(x, y, col);
  }

  TextureType texture;

  typename TextureType::uninitialized_bytes storage;
};

}
