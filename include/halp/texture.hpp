#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/container/vector.hpp>
#include <halp/controls.hpp>
#include <halp/modules.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>
#include <halp/texture_formats.hpp>

HALP_MODULE_EXPORT
namespace halp
{

template <static_string lit>
struct texture_input
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  struct rgba_row
  {
    unsigned char* bytes;
  };

  rgba_color get(int x, int y) noexcept
  {
    assert(x >= 0 && x < texture.width);
    assert(y >= 0 && y < texture.height);

    const int pixel_index = y * texture.width + x;
    const int byte_index = pixel_index * 4;

    auto* pixel_ptr = texture.bytes + byte_index;
    return {.r = pixel_ptr[0], .g = pixel_ptr[1], .b = pixel_ptr[2], .a = pixel_ptr[3]};
  }

  rgba_color get(int x, rgba_row y) noexcept
  {
    assert(x >= 0 && x < texture.width);
    const int byte_index = x * 4;
    auto* pixel_ptr = y.bytes + byte_index;
    return {.r = pixel_ptr[0], .g = pixel_ptr[1], .b = pixel_ptr[2], .a = pixel_ptr[3]};
  }

  rgba_row row(int y) noexcept
  {
    assert(y >= 0 && y < texture.height);
    return {texture.bytes + y * texture.width * 4};
  }

  rgba_texture texture;
};

template <static_string lit>
struct fixed_texture_input : texture_input<lit>
{
  int request_width{1};
  int request_height{1};
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
    if constexpr(requires { texture.bytes_per_pixel(); })
    {
      storage.resize(
          width * height * texture.bytes_per_pixel(), boost::container::default_init);
    }
    else
    {
      storage.resize(
          width * height * TextureType::bytes_per_pixel, boost::container::default_init);
    }
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

  void set(int x, int y, rgba_color col) noexcept { texture.set(x, y, col); }
  void set(int x, int y, rgb_color col) noexcept { texture.set(x, y, col); }

  TextureType texture;

  typename TextureType::uninitialized_bytes storage;
};

template <static_string lit>
using rgb_texture_output = texture_output<lit, rgb_texture>;

}
