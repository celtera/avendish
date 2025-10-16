#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <jit.common.h>
#include <max.jit.mop.h>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <cstring>
#include <algorithm>

namespace max::jitter
{

/**
 * Helper utilities for Jitter matrix operations
 */

// Matrix data type mapping
inline t_symbol* get_matrix_type_symbol(int bytes_per_pixel)
{
  switch(bytes_per_pixel)
  {
    case 1: return _jit_sym_char;
    case 2: return _jit_sym_long; // Actually 16-bit in Jitter
    case 4: return _jit_sym_float32;
    case 8: return _jit_sym_float64;
    default: return _jit_sym_char;
  }
}

// Get bytes per pixel from texture format
template<typename Field>
constexpr int get_texture_bytes_per_pixel()
{
  if constexpr(requires { typename Field::texture_type; })
  {
    using tex_type = typename Field::texture_type;
    if constexpr(requires { tex_type::bytes_per_pixel; })
      return tex_type::bytes_per_pixel;
  }
  // Default to RGBA8
  return 4;
}

// Matrix lock helper RAII wrapper
struct matrix_lock
{
  void* matrix{};
  void* prev_lock{};

  matrix_lock(void* m) : matrix(m)
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

/**
 * Convert Avendish texture to Jitter matrix
 * Creates or resizes output matrix as needed
 */
template<typename Field>
inline void texture_to_matrix(const Field& field, void* matrix)
{
  if(!matrix)
    return;

  const auto& tex = field.texture;

  // Skip if texture is invalid
  if(!tex.bytes || tex.width <= 0 || tex.height <= 0)
    return;

  // Skip if texture hasn't changed (optimization)
  if constexpr(requires { tex.changed; })
  {
    if(!tex.changed)
      return;
  }

  matrix_lock lock(matrix);

  // Get current matrix info
  t_jit_matrix_info info;
  jit_object_method(matrix, _jit_sym_getinfo, &info);

  // Update matrix dimensions if needed
  bool needs_resize = false;
  if(info.dimcount != 2 ||
     info.dim[0] != tex.width ||
     info.dim[1] != tex.height ||
     info.planecount != 4 ||
     info.type != _jit_sym_char)
  {
    needs_resize = true;
    info.dimcount = 2;
    info.dim[0] = tex.width;
    info.dim[1] = tex.height;
    info.planecount = 4; // RGBA
    info.type = _jit_sym_char;
    info.flags = 0;

    jit_object_method(matrix, _jit_sym_setinfo, &info);
  }

  // Get pointer to matrix data
  void* matrix_data = nullptr;
  jit_object_method(matrix, _jit_sym_getdata, &matrix_data);
  if(!matrix_data)
    return;

  // Copy texture data to matrix
  const int pixel_count = tex.width * tex.height;
  unsigned char* dst = static_cast<unsigned char*>(matrix_data);
  const unsigned char* src = static_cast<const unsigned char*>(tex.bytes);

  // Direct copy for RGBA char data
  std::memcpy(dst, src, pixel_count * 4);

  // Mark texture as no longer changed
  if constexpr(requires { tex.changed; })
  {
    const_cast<decltype(tex.changed)&>(tex.changed) = false;
  }
}

/**
 * Count texture inputs in a processor
 */
template<typename T>
constexpr size_t count_texture_inputs()
{
  return avnd::cpu_texture_input_introspection<T>::size;
}

/**
 * Count texture outputs in a processor
 */
template<typename T>
constexpr size_t count_texture_outputs()
{
  return avnd::cpu_texture_output_introspection<T>::size;
}

/**
 * Check if a processor has any texture ports
 */
template<typename T>
constexpr bool has_texture_ports()
{
  return count_texture_inputs<T>() > 0 || count_texture_outputs<T>() > 0;
}

} // namespace jitter
#define TITI 1234
