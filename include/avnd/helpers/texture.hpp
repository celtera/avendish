#pragma once
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/polyfill.hpp>
#include <avnd/helpers/static_string.hpp>
#include <boost/container/vector.hpp>

namespace avnd
{
using uninitialized_bytes = boost::container::vector<unsigned char>;
struct rgba_texture
{
  enum format
  {
    RGBA
  };
  unsigned char* bytes;
  int width;
  int height;
  bool changed;

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
};

struct rgba_color {
  uint8_t r, g, b, a;
};

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
    return {
        .r = pixel_ptr[0],
        .g = pixel_ptr[1],
        .b = pixel_ptr[2],
        .a = pixel_ptr[3]
    };
  }

  rgba_texture texture;
};

template <static_string lit>
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
    storage = rgba_texture::allocate(width, height);
    texture.width = width;
    texture.height = height;
    texture.changed = false;
    texture.bytes = storage.data();
  }

  void upload() noexcept
  {
    texture.changed = true;
  }

  void set(int x, int y, int r, int g, int b, int a = 255) noexcept
  {
    assert(x >= 0 && x < texture.width);
    assert(y >= 0 && y < texture.height);

    const int pixel_index = y * texture.width + x;
    const int byte_index = pixel_index * 4;

    auto* pixel_ptr = storage.data() + byte_index;
    pixel_ptr[0] = r;
    pixel_ptr[1] = g;
    pixel_ptr[2] = b;
    pixel_ptr[3] = a;
  }

  void set(int x, int y, rgba_color col) noexcept
  {
      assert(x >= 0 && x < texture.width);
      assert(y >= 0 && y < texture.height);

      const int pixel_index = y * texture.width + x;
      const int byte_index = pixel_index * 4;

      auto* pixel_ptr = storage.data() + byte_index;
      pixel_ptr[0] = col.r;
      pixel_ptr[1] = col.g;
      pixel_ptr[2] = col.b;
      pixel_ptr[3] = col.a;
  }

  rgba_texture texture;

  uninitialized_bytes storage;
};

}
