#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <jit.common.h>
#include <max.jit.mop.h>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <algorithm>
#include <boost/container/vector.hpp>
#include <avnd/binding/max/jitter_texture_format.hpp>
#include <avnd/binding/max/jitter_texture_conversion.hpp>

namespace max::jitter
{

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


template<typename Field>
inline void matrix_to_texture(
    void* matrix, Field& field,
    boost::container::vector<unsigned char>& temp_storage,
    const max_texture_spec::Format& spec)
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
  tex.width = width;
  tex.height = height;
  tex.bytes = convert_input_texture(matrix_data, width, height, planes, info.type, temp_storage, spec.format);
  tex.changed = true;
}

inline void resize_matrix(void* matrix, int new_width, int new_height, int planes, t_symbol* type)
{
  // Get current matrix info
  t_jit_matrix_info info;
  jit_object_method(matrix, _jit_sym_getinfo, &info);

  if(info.dimcount != 2 ||
     info.dim[0] != new_width ||
     info.dim[1] != new_height||
     info.planecount != planes ||
     info.type != type)
  {
    info.dimcount = 2;
    info.dim[0] = new_width;
    info.dim[1] = new_height;
    info.planecount = planes;
    info.type = type;
    info.flags = 0;

    jit_object_method(matrix, _jit_sym_setinfo, &info);
  }
}

template<avnd::cpu_texture_port Field>
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

  copy_texture_to_max(fmt, matrix_data, tex.bytes, tex.width, tex.height, tex.bytesize());

  // Mark texture as no longer changed
  if constexpr(requires { tex.changed; })
  {
    const_cast<decltype(tex.changed)&>(tex.changed) = false;
  }
}


inline void resize_buffer(void* matrix, int new_length, int planes, t_symbol* type)
{
  // Get current matrix info
  t_jit_matrix_info info;
  jit_object_method(matrix, _jit_sym_getinfo, &info);

  if(info.dimcount != 1 ||
     info.dim[0] != new_length ||
     info.planecount != planes ||
     info.type != type)
  {
    info.dimcount = 1;
    info.dim[0] = new_length;
    info.planecount = planes;
    info.type = type;
    info.flags = 0;

    jit_object_method(matrix, _jit_sym_setinfo, &info);
  }
}

template<avnd::buffer_port Field>
inline void buffer_to_matrix(const Field& field, void* matrix)
{
  if(!matrix)
    return;

  const auto& tex = field.buffer;
  if constexpr(requires { tex.changed; })
    if(!tex.changed)
      return;

  // Skip if texture is invalid
  if(!tex.bytes || tex.bytesize <= 0)
    return;

  using texture_type = std::decay_t<decltype(tex)>;
  resize_buffer(matrix, tex.bytesize, 1, _jit_sym_char);

  // Get pointer to matrix data
  void* matrix_data = nullptr;
  jit_object_method(matrix, _jit_sym_getdata, &matrix_data);
  if(!matrix_data)
    return;

  std::memcpy(matrix_data, tex.bytes, tex.bytesize);

  // Mark texture as no longer changed
  if constexpr(requires { tex.changed; })
  {
    const_cast<decltype(tex.changed)&>(tex.changed) = false;
  }
}


template<avnd::buffer_port Field>
inline void matrix_to_buffer(void* matrix, Field& field)
{
  if(!matrix)
    return;

  matrix_lock lock(matrix);

  t_jit_matrix_info info;
  jit_object_method(matrix, _jit_sym_getinfo, &info);

  if(info.dimcount == 0)
    return;

  // Get matrix dimensions
  int64_t num_elements = 1;
  for(int i = 0; i < info.dimcount; i++)
    num_elements *= info.dim[i];
  const int planes = info.planecount;

  if(num_elements == 0)
    return;

  // Get pointer to matrix data
  void* matrix_data = nullptr;
  jit_object_method(matrix, _jit_sym_getdata, &matrix_data);
  if(!matrix_data)
    return;

  auto& tex = field.buffer;
  if(info.type == _jit_sym_char) {
    tex.bytesize = num_elements;
    tex.bytes = reinterpret_cast<const char*>(matrix_data);
  }
  else if(info.type == _jit_sym_long) {
    tex.bytesize = num_elements * sizeof(long);
    tex.bytes = reinterpret_cast<const char*>(matrix_data);
  }
  else if(info.type == _jit_sym_float32) {
    tex.bytesize = num_elements * sizeof(float);
    tex.bytes = reinterpret_cast<const char*>(matrix_data);
  }
  else if(info.type == _jit_sym_float64) {
    tex.bytesize = num_elements * sizeof(double);
    tex.bytes = reinterpret_cast<const char*>(matrix_data);
  }
  tex.changed = true;
}

}
