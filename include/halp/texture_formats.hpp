#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/container/vector.hpp>
#include <halp/modules.hpp>
#include <halp/value_types.hpp>
#include <span>

HALP_MODULE_EXPORT
namespace halp
{

struct r8_texture
{
  static constexpr auto bytes_per_pixel = sizeof(unsigned char);
  using uninitialized_bytes = boost::container::vector<unsigned char>;
  enum format
  {
    R8
  };
  unsigned char* bytes;
  int width;
  int height;
  bool changed;

  constexpr auto bytesize() const noexcept { return width * height * bytes_per_pixel; }

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

struct r32f_texture
{
  static constexpr auto bytes_per_pixel = sizeof(float);
  using uninitialized_bytes = boost::container::vector<float>;
  enum format
  {
    R32F
  };
  float* bytes;
  int width;
  int height;
  bool changed;

  constexpr auto bytesize() const noexcept { return width * height * bytes_per_pixel; }

  /* FIXME the allocation should not be managed by the plug-in */
  static auto allocate(int width, int height)
  {
    using namespace boost::container;
    return uninitialized_bytes(width * height, default_init);
  }

  void update(float* data, int w, int h) noexcept
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
  static constexpr auto bytes_per_pixel = 4 * sizeof(unsigned char);
  using uninitialized_bytes = boost::container::vector<unsigned char>;
  enum format
  {
    RGBA
  };
  unsigned char* bytes;
  int width;
  int height;
  bool changed;

  constexpr auto bytesize() const noexcept { return width * height * bytes_per_pixel; }

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
  static constexpr auto bytes_per_pixel = 4 * sizeof(float);
  using uninitialized_bytes = boost::container::vector<float>;
  enum format
  {
    RGBA32F
  };
  float* bytes;
  int width;
  int height;
  bool changed;

  constexpr auto bytesize() const noexcept { return width * height * bytes_per_pixel; }

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
  static constexpr auto bytes_per_pixel = 3 * sizeof(unsigned char);
  using uninitialized_bytes = boost::container::vector<unsigned char>;
  enum format
  {
    RGB
  };

  unsigned char* bytes;
  int width;
  int height;
  bool changed;

  constexpr auto bytesize() const noexcept { return width * height * bytes_per_pixel; }

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


struct custom_texture_base
{
  using uninitialized_bytes = boost::container::vector<unsigned char>;
  unsigned char* bytes;
  int width;
  int height;
  bool changed;

  enum texture_format : uint8_t
  {
    RGBA8,
    BGRA8,
    R8,
    RG8,
    R16,
    RG16,
    RED_OR_ALPHA8,

    RGBA16F,
    RGBA32F,
    R16F,
    R32F,

    R8UI,
    R32UI,
    RG32UI,
    RGBA32UI,
  };

  static constexpr int component_size(texture_format format) noexcept
  {
    switch(format)
    {
      case RGBA8:
      case BGRA8:
        return 1;
      case R8:
        return 1;
      case RG8:
        return 1;
      case R16:
        return 2;
      case RG16:
        return 2;
      case RED_OR_ALPHA8:
        return 1;
      case RGBA16F:
        return 2;
      case RGBA32F:
        return 4;
      case R16F:
        return 2;
      case R32F:
        return 4;
      case R8UI:
        return 1;
      case R32UI:
        return 4;
      case RG32UI:
        return 4;
      case RGBA32UI:
        return 4;
      default:
        return 1;
    }
  }
  static constexpr int components(texture_format format) noexcept
  {
    switch(format)
    {
      case RGBA8:
      case BGRA8:
        return 4;
      case R8:
        return 1;
      case RG8:
        return 2;
      case R16:
        return 1;
      case RG16:
        return 2;
      case RED_OR_ALPHA8:
        return 1;
      case RGBA16F:
        return 4;
      case RGBA32F:
        return 4;
      case R16F:
        return 1;
      case R32F:
        return 1;
      case R8UI:
        return 1;
      case R32UI:
        return 1;
      case RG32UI:
        return 2;
      case RGBA32UI:
        return 4;
      default:
        return 1;
    }
  }

  void update(unsigned char* data, int w, int h) noexcept
  {
    bytes = data;
    width = w;
    height = h;
    changed = true;
  }
};

// Use when you want your plugin to request a texture format
// from the host
struct custom_texture : custom_texture_base
{
  using custom_texture_base::component_size;
  using custom_texture_base::components;
  using custom_texture_base::update;

  texture_format request_format = RGBA8;

  auto bytesize() const noexcept { return bytes_per_pixel() * width * height; }
  auto component_size() const noexcept { return bytes_per_pixel() * width * height; }
  int bytes_per_pixel() const noexcept
  {
    return component_size(request_format) * components(request_format);
  }

  /* FIXME the allocation should not be managed by the plug-in */
  auto allocate(int width, int height)
  {
    using namespace boost::container;
    return uninitialized_bytes(bytesize(), default_init);
  }
};

// Use when you want your plugin to receive whatever format
// the host chooses
struct custom_variable_texture : custom_texture_base
{
  using custom_texture_base::component_size;
  using custom_texture_base::components;
  using custom_texture_base::update;

  texture_format format = RGBA8;

  auto bytesize() const noexcept { return bytes_per_pixel() * width * height; }
  auto component_size() const noexcept { return bytes_per_pixel() * width * height; }
  int bytes_per_pixel() const noexcept
  {
    return component_size(format) * components(format);
  }

  /* FIXME the allocation should not be managed by the plug-in */
  auto allocate(int width, int height)
  {
    using namespace boost::container;
    return uninitialized_bytes(bytesize(), default_init);
  }
};

}
