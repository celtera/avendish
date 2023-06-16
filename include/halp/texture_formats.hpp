#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/container/vector.hpp>
#include <halp/value_types.hpp>

namespace halp
{
struct r8_texture
{
  using uninitialized_bytes = boost::container::vector<unsigned char>;
  enum format
  {
    R8
  };
  unsigned char* bytes;
  int width;
  int height;
  bool changed;

  constexpr auto bytesize() const noexcept
  {
    return width * height * sizeof(unsigned char);
  }

  /* FIXME the allocation should not be managed by the plug-in */
  static auto allocate(int width, int height)
  {
    using namespace boost::container;
    return uninitialized_bytes(width * height, default_init);
  }

  void update(unsigned char* data, int w, int h) noexcept
  {
    bytes = data;
    width = w;
    height = h;
    changed = true;
  }

  void set(int x, int y, int r) noexcept
  {
    assert(x >= 0 && x < width);
    assert(y >= 0 && y < height);

    const int pixel_index = y * width + x;
    const int byte_index = pixel_index;

    auto* pixel_ptr = bytes + byte_index;
    pixel_ptr[0] = r;
  }

  void set(int x, int y, r_color col) noexcept
  {
    assert(x >= 0 && x < width);
    assert(y >= 0 && y < height);

    const int pixel_index = y * width + x;
    const int byte_index = pixel_index;

    auto* pixel_ptr = bytes + byte_index;
    pixel_ptr[0] = col.r;
  }
};

struct rgba_texture
{
  using uninitialized_bytes = boost::container::vector<unsigned char>;
  enum format
  {
    RGBA
  };
  unsigned char* bytes;
  int width;
  int height;
  bool changed;

  constexpr auto bytesize() const noexcept
  {
    return width * height * 4 * sizeof(unsigned char);
  }

  /* FIXME the allocation should not be managed by the plug-in */
  static auto allocate(int width, int height)
  {
    using namespace boost::container;
    return uninitialized_bytes(width * height * 4, default_init);
  }

  void update(unsigned char* data, int w, int h) noexcept
  {
    bytes = data;
    width = w;
    height = h;
    changed = true;
  }

  void set(int x, int y, int r, int g, int b, int a = 255) noexcept
  {
    assert(x >= 0 && x < width);
    assert(y >= 0 && y < height);

    const int pixel_index = y * width + x;
    const int byte_index = pixel_index * 4;

    auto* pixel_ptr = bytes + byte_index;
    pixel_ptr[0] = r;
    pixel_ptr[1] = g;
    pixel_ptr[2] = b;
    pixel_ptr[3] = a;
  }

  void set(int x, int y, rgba_color col) noexcept
  {
    assert(x >= 0 && x < width);
    assert(y >= 0 && y < height);

    const int pixel_index = y * width + x;
    const int byte_index = pixel_index * 4;

    auto* pixel_ptr = bytes + byte_index;
    pixel_ptr[0] = col.r;
    pixel_ptr[1] = col.g;
    pixel_ptr[2] = col.b;
    pixel_ptr[3] = col.a;
  }
};

struct rgba32f_texture
{
  using uninitialized_bytes = boost::container::vector<float>;
  enum format
  {
    RGBA32F
  };
  float* bytes;
  int width;
  int height;
  bool changed;

  constexpr auto bytesize() const noexcept { return width * height * 4 * sizeof(float); }

  /* FIXME the allocation should not be managed by the plug-in */
  static auto allocate(int width, int height)
  {
    using namespace boost::container;
    return uninitialized_bytes(width * height * 4, default_init);
  }

  void update(float* data, int w, int h) noexcept
  {
    bytes = data;
    width = w;
    height = h;
    changed = true;
  }

  void set(int x, int y, int r, int g, int b, int a = 255) noexcept
  {
    assert(x >= 0 && x < width);
    assert(y >= 0 && y < height);

    const int pixel_index = y * width + x;
    const int byte_index = pixel_index * 4;

    auto* pixel_ptr = bytes + byte_index;
    pixel_ptr[0] = r / 255.f;
    pixel_ptr[1] = g / 255.f;
    pixel_ptr[2] = b / 255.f;
    pixel_ptr[3] = a / 255.f;
  }

  void set(int x, int y, rgba_color col) noexcept
  {
    assert(x >= 0 && x < width);
    assert(y >= 0 && y < height);

    const int pixel_index = y * width + x;
    const int byte_index = pixel_index * 4;

    auto* pixel_ptr = bytes + byte_index;
    pixel_ptr[0] = col.r / 255.f;
    pixel_ptr[1] = col.g / 255.f;
    pixel_ptr[2] = col.b / 255.f;
    pixel_ptr[3] = col.a / 255.f;
  }

  void set(int x, int y, rgba32f_color col) noexcept
  {
    assert(x >= 0 && x < width);
    assert(y >= 0 && y < height);

    const int pixel_index = y * width + x;
    const int byte_index = pixel_index * 4;

    auto* pixel_ptr = bytes + byte_index;
    pixel_ptr[0] = col.r;
    pixel_ptr[1] = col.g;
    pixel_ptr[2] = col.b;
    pixel_ptr[3] = col.a;
  }
};

struct rgb_texture
{
  using uninitialized_bytes = boost::container::vector<unsigned char>;
  enum format
  {
    RGB
  };

  unsigned char* bytes;
  int width;
  int height;
  bool changed;

  constexpr auto bytesize() const noexcept
  {
    return width * height * 3 * sizeof(unsigned char);
  }

  /* FIXME the allocation should not be managed by the plug-in */
  static auto allocate(int width, int height)
  {
    using namespace boost::container;
    return uninitialized_bytes(width * height * 3, default_init);
  }

  void update(unsigned char* data, int w, int h) noexcept
  {
    bytes = data;
    width = w;
    height = h;
    changed = true;
  }
};

}
