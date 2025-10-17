#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <jit.common.h>
#include <max.jit.mop.h>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace max::jitter
{

struct max_texture_spec {
  struct Format {
    int planes = 1;
    t_symbol* type = _jit_sym_char;
    Format() = delete;
    Format(const Format&) = delete;
    Format(Format&&) = delete;
    Format(int p , t_symbol* s): planes{p}, type{s} {}
  };
  static const inline Format ARGB8         { 4, _jit_sym_char };
  static const inline Format RGBA8         { 4, _jit_sym_char };
  static const inline Format BGRA8         { 4, _jit_sym_char };
  static const inline Format R8            { 1, _jit_sym_char };
  static const inline Format RG8           { 2, _jit_sym_char };
  static const inline Format R16           { 1, _jit_sym_long }; // :(
  static const inline Format RG16          { 2, _jit_sym_long }; // :(
  static const inline Format RED_OR_ALPHA8 { 1, _jit_sym_char };
  static const inline Format RGBA16F       { 4, _jit_sym_float32 }; // :(
  static const inline Format RGBA32F       { 4, _jit_sym_float32 };
  static const inline Format ARGB32F       { 4, _jit_sym_float32 };
  static const inline Format R16F          { 1, _jit_sym_float32 }; // :(
  static const inline Format R32F          { 1, _jit_sym_float32 };
  static const inline Format RGB10A2       { 4, _jit_sym_char }; // :(
  static const inline Format D16           { 1, _jit_sym_long }; // :(
  static const inline Format D24           { 1, _jit_sym_long }; // :(
  static const inline Format D24S8         { 1, _jit_sym_long }; // :(
  static const inline Format D32F          { 1, _jit_sym_float32 };
  static const inline Format RGB           { 3, _jit_sym_char };
};

template <typename F>
  requires std::is_enum_v<F>
constexpr const max_texture_spec::Format& texture_spec(F f) noexcept
{
  if constexpr(requires { F::RGBA; } || requires { F::RGBA8; })
    if(f == F::RGBA8)
      return max_texture_spec::RGBA8;
  if constexpr(requires { F::ARGB; } || requires { F::ARGB8; })
    if(f == F::ARGB8)
      return max_texture_spec::ARGB8;
  if constexpr(requires { F::BGRA; } || requires { F::BGRA8; })
    if(f == F::BGRA8)
      return max_texture_spec::BGRA8;
  if constexpr(requires { F::R8; } || requires { F::GRAYSCALE; })
    if(f == F::R8)
      return max_texture_spec::R8;
  if constexpr(requires { F::RG8; })
    if(f == F::RG8)
      return max_texture_spec::RG8;
  if constexpr(requires { F::R16; })
    if(f == F::R16)
      return max_texture_spec::R16;
  if constexpr(requires { F::RG16; })
    if(f == F::RG16)
      return max_texture_spec::RG16;
  if constexpr(requires { F::RED_OR_ALPHA8; })
    if(f == F::RED_OR_ALPHA8)
      return max_texture_spec::RED_OR_ALPHA8;
  if constexpr(requires { F::RGBA16F; })
    if(f == F::RGBA16F)
      return max_texture_spec::RGBA16F;
  if constexpr(requires { F::RGBA32F; })
    if(f == F::RGBA32F)
      return max_texture_spec::RGBA32F;
  if constexpr(requires { F::ARGB32F; })
    if(f == F::ARGB32F)
      return max_texture_spec::ARGB32F;
  if constexpr(requires { F::R16F; })
    if(f == F::R16F)
      return max_texture_spec::R16F;
  if constexpr(requires { F::R32F; })
    if(f == F::R32F)
      return max_texture_spec::R32F;
  if constexpr(requires { F::RGB10A2; })
    if(f == F::RGB10A2)
      return max_texture_spec::RGB10A2;
  if constexpr(requires { F::D16; })
    if(f == F::D16)
      return max_texture_spec::D16;
  if constexpr(requires { F::D24; })
    if(f == F::D24)
      return max_texture_spec::D24;
  if constexpr(requires { F::D24S8; })
    if(f == F::D24S8)
      return max_texture_spec::D24S8;
  if constexpr(requires { F::D32F; })
    if(f == F::D32F)
      return max_texture_spec::D32F;
  if constexpr(requires { F::RGB; })
    if(f == F::RGB)
      return max_texture_spec::RGB;

  return max_texture_spec::RGBA8;
}

template <typename F>
const max_texture_spec::Format& texture_spec() noexcept
{
  if constexpr(requires { std::string_view{F::format()}; })
  {
    constexpr std::string_view fmt = F::format();

    if(fmt == "rgba" || fmt == "rgba8")
      return max_texture_spec::RGBA8;
    if(fmt == "argb" || fmt == "argb8")
      return max_texture_spec::ARGB8;
    else if(fmt == "bgra" || fmt == "bgra8")
      return max_texture_spec::BGRA8;
    else if(fmt == "r8")
      return max_texture_spec::R8;
    else if(fmt == "rg8")
      return max_texture_spec::RG8;
    else if(fmt == "r16")
      return max_texture_spec::R16;
    else if(fmt == "rg16")
      return max_texture_spec::RG16;
    else if(fmt == "red_or_alpha8")
      return max_texture_spec::RED_OR_ALPHA8;
    else if(fmt == "rgba16f")
      return max_texture_spec::RGBA16F;
    else if(fmt == "rgba32f")
      return max_texture_spec::RGBA32F;
    else if(fmt == "argb32f")
      return max_texture_spec::ARGB32F;
    else if(fmt == "r16f")
      return max_texture_spec::R16F;
    else if(fmt == "r32")
      return max_texture_spec::R32F;
    else if(fmt == "rgb10a2")
      return max_texture_spec::RGB10A2;
    else if(fmt == "d16")
      return max_texture_spec::D16;
    else if(fmt == "d24")
      return max_texture_spec::D24;
    else if(fmt == "d24s8")
      return max_texture_spec::D24S8;
    else if(fmt == "d32f")
      return max_texture_spec::D32F;
    else if(fmt == "rgb")
      return max_texture_spec::RGB;
    else
      return max_texture_spec::RGBA8;
  }
  else if constexpr(std::is_enum_v<typename F::format>)
  {
    if constexpr(requires { F::RGBA; } || requires { F::RGBA8; })
      return max_texture_spec::RGBA8;
    if constexpr(requires { F::ARGB; } || requires { F::ARGB8; })
      return max_texture_spec::ARGB8;
    else if constexpr(requires { F::BGRA; } || requires { F::BGRA8; })
      return max_texture_spec::BGRA8;
    else if constexpr(requires { F::R8; } || requires { F::GRAYSCALE; })
      return max_texture_spec::R8;
    else if constexpr(requires { F::RG8; })
      return max_texture_spec::RG8;
    else if constexpr(requires { F::R16; })
      return max_texture_spec::R16;
    else if constexpr(requires { F::RG16; })
      return max_texture_spec::RG16;
    else if constexpr(requires { F::RED_OR_ALPHA8; })
      return max_texture_spec::RED_OR_ALPHA8;
    else if constexpr(requires { F::RGBA16F; })
      return max_texture_spec::RGBA16F;
    else if constexpr(requires { F::RGBA32F; })
      return max_texture_spec::RGBA32F;
    else if constexpr(requires { F::ARGB32F; })
      return max_texture_spec::ARGB32F;
    else if constexpr(requires { F::R16F; })
      return max_texture_spec::R16F;
    else if constexpr(requires { F::R32F; })
      return max_texture_spec::R32F;
    else if constexpr(requires { F::RGB10A2; })
      return max_texture_spec::RGB10A2;
    else if constexpr(requires { F::D16; })
      return max_texture_spec::D16;
    else if constexpr(requires { F::D24; })
      return max_texture_spec::D24;
    else if constexpr(requires { F::D24S8; })
      return max_texture_spec::D24S8;
    else if constexpr(requires { F::D32F; })
      return max_texture_spec::D32F;
    else if constexpr(requires { F::RGB; })
      return max_texture_spec::RGB;
    else
      return max_texture_spec::RGBA8;
  }
}

template <avnd::cpu_texture Tex>
const max_texture_spec::Format& texture_spec(const Tex& t) noexcept
{
  if constexpr(avnd::cpu_dynamic_format_texture<Tex>)
    return texture_spec(t.format);
  else
    return texture_spec<Tex>();
}

template<typename Field>
static t_symbol* get_jitter_type_for_parameter()
{
  using value_type = std::decay_t<decltype(Field::value)>;

  if constexpr(std::is_same_v<char, value_type>)
    return _jit_sym_char;
  else if constexpr(std::is_same_v<unsigned char, value_type>)
    return _jit_sym_char;
  else if constexpr(std::is_same_v<signed char, value_type>)
    return _jit_sym_char;
  else if constexpr(std::is_integral_v<value_type>)
    return _jit_sym_long;
  else if constexpr(std::is_same_v<float, value_type>)
    return _jit_sym_float32;
  else if constexpr(std::is_same_v<double, value_type>)
    return _jit_sym_float64;
  else if constexpr(avnd::string_ish<value_type>)
    return _jit_sym_symbol;
  else
    return _jit_sym_atom;
}

struct matrix_lock
{
  void* matrix{};
  void* prev_lock{};

  explicit matrix_lock(void* m) : matrix(m)
  {
    if(matrix)
      prev_lock = jit_object_method(matrix, _jit_sym_lock, 1);
  }

  ~matrix_lock()
  {
    if(matrix)
      jit_object_method(matrix, _jit_sym_lock, prev_lock);
  }

  matrix_lock(const matrix_lock&) = delete;
  matrix_lock& operator=(const matrix_lock&) = delete;
  matrix_lock(matrix_lock&&) noexcept = delete;
  matrix_lock& operator=(matrix_lock&&) noexcept = delete;
};

/**
 * Convert Jitter matrix to Avendish texture
 * Handles char and float32 matrix types
 */
template<typename Field>
inline void matrix_to_texture(void* matrix, Field& field)
{
  if(!matrix)
    return;

  matrix_lock lock(matrix);

  t_jit_matrix_info info;
  jit_object_method(matrix, _jit_sym_getinfo, &info);

  // Get matrix dimensions
  const int width = (info.dimcount > 0) ? info.dim[0] : 0;
  const int height = (info.dimcount > 1) ? info.dim[1] : 0;
  const int planes = info.planecount;

  if(width == 0 || height == 0)
    return;

  // Get pointer to matrix data
  void* matrix_data = nullptr;
  jit_object_method(matrix, _jit_sym_getdata, &matrix_data);
  if(!matrix_data)
    return;

  auto& tex = field.texture;

  // Resize texture if needed
  bool size_changed = false;
  if(tex.width != width || tex.height != height)
  {
    size_changed = true;
    tex.width = width;
    tex.height = height;

    // Reallocate texture storage if using halp::texture types
    if constexpr(requires { field.create(width, height); })
    {
      field.create(width, height);
    }
    else if constexpr(requires { tex.resize(width, height); })
    {
      tex.resize(width, height);
    }
  }

  // Ensure we have a valid bytes pointer
  if(!tex.bytes)
  {
    // Try to allocate if possible
    if constexpr(requires { field.allocate(width, height); })
    {
      field.allocate(width, height);
    }
    else
    {
      // Can't allocate, bail out
      return;
    }
  }

  // Convert matrix data to RGBA texture format
  const int pixel_count = width * height;
  unsigned char* dst = static_cast<unsigned char*>(tex.bytes);

  if(info.type == _jit_sym_char)
  {
    // Char matrix (most common for images)
    unsigned char* src = static_cast<unsigned char*>(matrix_data);

    if(planes == 4)
    {
      // Direct copy for RGBA
      std::memcpy(dst, src, pixel_count * 4);
    }
    else if(planes == 3)
    {
      // RGB -> RGBA
      for(int i = 0; i < pixel_count; i++)
      {
        dst[i * 4 + 0] = src[i * 3 + 0]; // R
        dst[i * 4 + 1] = src[i * 3 + 1]; // G
        dst[i * 4 + 2] = src[i * 3 + 2]; // B
        dst[i * 4 + 3] = 255;            // A
      }
    }
    else if(planes == 1)
    {
      // Grayscale -> RGBA
      for(int i = 0; i < pixel_count; i++)
      {
        unsigned char val = src[i];
        dst[i * 4 + 0] = val; // R
        dst[i * 4 + 1] = val; // G
        dst[i * 4 + 2] = val; // B
        dst[i * 4 + 3] = 255; // A
      }
    }
  }
  else if(info.type == _jit_sym_float32)
  {
    // Float32 matrix
    float* src = static_cast<float*>(matrix_data);

    // Convert float [0..1] to byte [0..255]
    auto float_to_byte = [](float f) -> unsigned char {
      return static_cast<unsigned char>(std::clamp(f * 255.0f, 0.0f, 255.0f));
    };

    if(planes == 4)
    {
      for(int i = 0; i < pixel_count; i++)
      {
        dst[i * 4 + 0] = float_to_byte(src[i * 4 + 0]); // R
        dst[i * 4 + 1] = float_to_byte(src[i * 4 + 1]); // G
        dst[i * 4 + 2] = float_to_byte(src[i * 4 + 2]); // B
        dst[i * 4 + 3] = float_to_byte(src[i * 4 + 3]); // A
      }
    }
    else if(planes == 3)
    {
      for(int i = 0; i < pixel_count; i++)
      {
        dst[i * 4 + 0] = float_to_byte(src[i * 3 + 0]); // R
        dst[i * 4 + 1] = float_to_byte(src[i * 3 + 1]); // G
        dst[i * 4 + 2] = float_to_byte(src[i * 3 + 2]); // B
        dst[i * 4 + 3] = 255;                            // A
      }
    }
    else if(planes == 1)
    {
      for(int i = 0; i < pixel_count; i++)
      {
        unsigned char val = float_to_byte(src[i]);
        dst[i * 4 + 0] = val; // R
        dst[i * 4 + 1] = val; // G
        dst[i * 4 + 2] = val; // B
        dst[i * 4 + 3] = 255; // A
      }
    }
  }

  // Mark texture as changed
  tex.changed = true;
}

inline void resize_matrix(void* matrix, int new_width, int new_height, int planes,  t_symbol* type)
{
  // Get current matrix info
  t_jit_matrix_info info;
  jit_object_method(matrix, _jit_sym_getinfo, &info);

  bool needs_resize = false;
  if(info.dimcount != 2 ||
     info.dim[0] != new_width ||
     info.dim[1] != new_height||
     info.planecount != planes ||
     info.type != type)
  {
    needs_resize = true;
    info.dimcount = 2;
    info.dim[0] = new_width;
    info.dim[1] = new_height;
    info.planecount = planes;
    info.type = type;
    info.flags = 0;

    jit_object_method(matrix, _jit_sym_setinfo, &info);
  }
}

inline
void copy_texture(const max_texture_spec::Format& fmt, void* matrix_data, void* tex_bytes, int width, int height, int tex_bytesize)
{
  const int pixel_count = width * height;

  if(fmt.type == _jit_sym_char)
  {
    unsigned char* dst = static_cast<unsigned char*>(matrix_data);
    const unsigned char* src = static_cast<const unsigned char*>(tex_bytes);
    const int bytesize = fmt.planes * sizeof(char);

    if(&fmt == &max_texture_spec::RGBA8)
    {
      for(int i = 0, N = pixel_count * 4; i < N; i += 4) {
        dst[i + 0] = src[i + 3];
        dst[i + 1] = src[i + 0];
        dst[i + 2] = src[i + 1];
        dst[i + 3] = src[i + 2];
      }
    }
    else if(&fmt == &max_texture_spec::BGRA8)
    {
      for(int i = 0, N = pixel_count * 4; i < N; i += 4) {
        dst[i + 0] = src[i + 3];
        dst[i + 1] = src[i + 2];
        dst[i + 2] = src[i + 1];
        dst[i + 3] = src[i + 0];
      }
    }
    else
    {
      std::memcpy(dst, src, std::min(pixel_count * bytesize, tex_bytesize));
    }
  }
  else if(fmt.type == _jit_sym_float32)
  {
    float* dst = static_cast<float*>(matrix_data);
    const float* src = static_cast<const float*>(tex_bytes);
    const int bytesize = fmt.planes * sizeof(float);
    if(&fmt == &max_texture_spec::RGBA32F)
    {
      for(int i = 0, N = pixel_count * 4; i < N; i += 4) {
        dst[i + 0] = src[i + 3];
        dst[i + 1] = src[i + 0];
        dst[i + 2] = src[i + 1];
        dst[i + 3] = src[i + 2];
      }
    }
    else if(&fmt == &max_texture_spec::RGBA32F)
    {
      for(int i = 0, N = pixel_count * 4; i < N; i += 4) {
        dst[i + 0] = src[i + 3];
        dst[i + 1] = src[i + 0];
        dst[i + 2] = src[i + 1];
        dst[i + 3] = src[i + 2];
      }
    }
    else
    {
      std::memcpy(dst, src, std::min(pixel_count * bytesize, tex_bytesize));
    }
  }
}

template<typename Field>
inline void texture_to_matrix(const Field& field, void* matrix)
{
  if(!matrix)
    return;

  const auto& tex = field.texture;
  if constexpr(requires { tex.changed; })
    if(!tex.changed)
      return;

  // Skip if texture is invalid
  if(!tex.bytes || tex.width <= 0 || tex.height <= 0)
    return;

  using texture_type = std::decay_t<decltype(tex)>;
  const auto& fmt = texture_spec(tex);
  resize_matrix(matrix, tex.width, tex.height, fmt.planes, fmt.type);

  // Get pointer to matrix data
  void* matrix_data = nullptr;
  jit_object_method(matrix, _jit_sym_getdata, &matrix_data);
  if(!matrix_data)
    return;

  copy_texture(fmt, matrix_data, tex.bytes, tex.width, tex.height, tex.bytesize());

  // Mark texture as no longer changed
  if constexpr(requires { tex.changed; })
  {
    const_cast<decltype(tex.changed)&>(tex.changed) = false;
  }
}

}
