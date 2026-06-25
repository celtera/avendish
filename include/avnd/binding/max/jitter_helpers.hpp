#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <jit.common.h>
#include <max.jit.mop.h>
#include <avnd/concepts/gfx.hpp>
#include <avnd/concepts/tensor.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <algorithm>
#include <boost/container/vector.hpp>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include <avnd/binding/max/jitter_texture_format.hpp>
#include <avnd/binding/max/jitter_texture_conversion.hpp>

namespace max::jitter
{

inline constexpr int jitter_max_dimcount = 32;

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

  // Jitter rows are padded to dimstride[1]; convert_input_texture reads a
  // tightly-packed buffer, so re-pack a padded matrix first.
  void* src = matrix_data;
  static thread_local boost::container::vector<unsigned char> packed;
  const long packed_row = (long)width * info.dimstride[0];
  if(info.dimstride[1] != packed_row)
  {
    packed.resize(std::size_t(packed_row) * height);
    const auto* s = static_cast<const unsigned char*>(matrix_data);
    for(int y = 0; y < height; ++y)
      std::memcpy(packed.data() + std::size_t(y) * packed_row,
                  s + std::size_t(y) * info.dimstride[1], packed_row);
    src = packed.data();
  }

  auto& tex = field.texture;
  tex.width = width;
  tex.height = height;
  // convert_input_texture works in bytes; tex.bytes may be a typed pointer
  // (e.g. float* for r32f/rgba32f), so keep the result as bytes and cast once.
  unsigned char* converted
      = convert_input_texture(src, width, height, planes, info.type, temp_storage, spec.format);
  // If no conversion happened the result aliases the transient `packed` scratch;
  // copy into the per-field temp_storage so it survives until the processor runs.
  if(converted == packed.data())
  {
    temp_storage.assign(packed.begin(), packed.end());
    converted = temp_storage.data();
  }
  tex.bytes = reinterpret_cast<std::decay_t<decltype(tex.bytes)>>(converted);
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

  // Jitter pads each matrix row to dimstride[1]; copy_texture_to_max writes a
  // tightly-packed buffer, so write into a packed scratch and re-stride into the
  // matrix when the row is padded (otherwise the bottom rows are dropped).
  t_jit_matrix_info info;
  jit_object_method(matrix, _jit_sym_getinfo, &info);
  const long packed_row = (long)tex.width * info.dimstride[0];
  if(info.dimstride[1] == packed_row)
  {
    copy_texture_to_max(fmt, matrix_data, tex.bytes, tex.width, tex.height, tex.bytesize());
  }
  else
  {
    static thread_local boost::container::vector<unsigned char> scratch;
    scratch.resize(std::size_t(packed_row) * tex.height);
    copy_texture_to_max(fmt, scratch.data(), tex.bytes, tex.width, tex.height, tex.bytesize());
    auto* d = static_cast<unsigned char*>(matrix_data);
    for(int y = 0; y < tex.height; ++y)
      std::memcpy(d + std::size_t(y) * info.dimstride[1],
                  scratch.data() + std::size_t(y) * packed_row, packed_row);
  }

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

template<avnd::cpu_raw_buffer_port Field>
inline void buffer_to_matrix(const Field& field, void* matrix)
{
  if(!matrix)
    return;

  const auto& buf = field.buffer;
  if constexpr(requires { buf.changed; })
    if(!buf.changed)
      return;

  // Skip if buffer is invalid
  if(!buf.raw_data || buf.byte_size <= 0)
    return;

  resize_buffer(matrix, buf.byte_size, 1, _jit_sym_char);

  // Get pointer to matrix data
  void* matrix_data = nullptr;
  jit_object_method(matrix, _jit_sym_getdata, &matrix_data);
  if(!matrix_data)
    return;

  std::memcpy(matrix_data, buf.raw_data, buf.byte_size);

  // Mark texture as no longer changed
  if constexpr(requires { buf.changed; })
  {
    const_cast<decltype(buf.changed)&>(buf.changed) = false;
  }
}

template<avnd::cpu_typed_buffer_port Field>
inline void buffer_to_matrix(const Field& field, void* matrix)
{
  if(!matrix)
    return;

  const auto& buf = field.buffer;
  if constexpr(requires { buf.changed; })
    if(!buf.changed)
      return;

  // Skip if texture is invalid
  if(!buf.elements || buf.count <= 0)
    return;

  using buffer_type = std::decay_t<decltype(buf)>;
  using value_type = std::decay_t<std::remove_pointer_t<std::decay_t<decltype(buf.elements[0])>>>;
  t_symbol* sym{};
  if constexpr(sizeof(value_type) == 1)
    sym = _jit_sym_char;
  else if constexpr(std::is_same_v<value_type, float>)
    sym = _jit_sym_float32;
  else if constexpr(std::is_same_v<value_type, double>)
    sym = _jit_sym_float64;
  else if constexpr(std::is_integral_v<value_type> && sizeof(value_type) == sizeof(long))
    sym = _jit_sym_long;

  if(!sym)
    return;

  resize_buffer(matrix, buf.count, 1, sym);

  // Get pointer to matrix data
  void* matrix_data = nullptr;
  jit_object_method(matrix, _jit_sym_getdata, &matrix_data);
  if(!matrix_data)
    return;

  std::memcpy(matrix_data, buf.elements, buf.count * sizeof(value_type));

  // Mark texture as no longer changed
  if constexpr(requires { buf.changed; })
  {
    const_cast<decltype(buf.changed)&>(buf.changed) = false;
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
    tex.byte_size = num_elements;
    tex.raw_data = reinterpret_cast<unsigned char*>(matrix_data);
  }
  else if(info.type == _jit_sym_long) {
    tex.byte_size = num_elements * sizeof(long);
    tex.raw_data = reinterpret_cast<unsigned char*>(matrix_data);
  }
  else if(info.type == _jit_sym_float32) {
    tex.byte_size = num_elements * sizeof(float);
    tex.raw_data = reinterpret_cast<unsigned char*>(matrix_data);
  }
  else if(info.type == _jit_sym_float64) {
    tex.byte_size = num_elements * sizeof(double);
    tex.raw_data = reinterpret_cast<unsigned char*>(matrix_data);
  }
  tex.changed = true;
}

template <typename T>
inline t_symbol* jitter_type_for_element() noexcept
{
  constexpr avnd::dtype_descriptor d = avnd::dtype_of<T>;
  if constexpr(d.code == static_cast<uint8_t>(avnd::dtype_code::Float))
  {
    if constexpr(d.bits == 32)
      return _jit_sym_float32;
    else if constexpr(d.bits == 64)
      return _jit_sym_float64;
    else
      return _jit_sym_float32;
  }
  else if constexpr(d.code == static_cast<uint8_t>(avnd::dtype_code::Int)
                    || d.code == static_cast<uint8_t>(avnd::dtype_code::UInt))
  {
    if constexpr(d.bits == 8)
      return _jit_sym_char;
    else
      return _jit_sym_long;
  }
  else if constexpr(d.code == static_cast<uint8_t>(avnd::dtype_code::Bool))
  {
    return _jit_sym_char;
  }
  else
  {
    return _jit_sym_float32;
  }
}

inline std::size_t jitter_element_size(t_symbol* type) noexcept
{
  if(type == _jit_sym_char)
    return 1;
  if(type == _jit_sym_long)
    return sizeof(long);
  if(type == _jit_sym_float32)
    return sizeof(float);
  if(type == _jit_sym_float64)
    return sizeof(double);
  return 0;
}

inline bool resize_tensor_matrix(
    void* matrix,
    const std::size_t* shape_data,
    std::size_t rank,
    t_symbol* type)
{
  if(!matrix)
    return false;
  if(rank == 0 || rank > static_cast<std::size_t>(jitter_max_dimcount))
    return false;

  t_jit_matrix_info info;
  jit_object_method(matrix, _jit_sym_getinfo, &info);

  const int new_dimcount = static_cast<int>(rank);
  bool needs_update = info.dimcount != new_dimcount
                      || info.planecount != 1
                      || info.type != type;
  if(!needs_update)
  {
    for(int i = 0; i < new_dimcount; ++i)
    {
      if(info.dim[i] != static_cast<long>(shape_data[i]))
      {
        needs_update = true;
        break;
      }
    }
  }

  if(needs_update)
  {
    info.dimcount = new_dimcount;
    for(int i = 0; i < new_dimcount; ++i)
      info.dim[i] = static_cast<long>(shape_data[i]);
    info.planecount = 1;
    info.type = type;
    info.flags = 0;
    jit_object_method(matrix, _jit_sym_setinfo, &info);
  }

  return true;
}

template <avnd::tensor_port Field>
inline void tensor_to_matrix(const Field& field, void* matrix)
{
  if(!matrix)
    return;

  using value_type = std::remove_cvref_t<decltype(Field::value)>;
  if constexpr(avnd::tensor_like<value_type>)
  {
    using element_type = avnd::tensor_element<value_type>;

    const auto* src = avnd::data_of(field.value);
    if(!src)
      return;

    auto shape_range = avnd::shape_of(field.value);
    const std::size_t rank = std::ranges::size(shape_range);
    if(rank == 0 || rank > static_cast<std::size_t>(jitter_max_dimcount))
      return;

    std::size_t shape_buf[jitter_max_dimcount];
    std::size_t total = 1;
    std::size_t i = 0;
    for(auto extent : shape_range)
    {
      if(i >= rank)
        break;
      shape_buf[i] = static_cast<std::size_t>(extent);
      total *= shape_buf[i];
      ++i;
    }

    if(total == 0)
      return;

    t_symbol* type = jitter_type_for_element<element_type>();
    if(!resize_tensor_matrix(matrix, shape_buf, rank, type))
      return;

    void* matrix_data = nullptr;
    jit_object_method(matrix, _jit_sym_getdata, &matrix_data);
    if(!matrix_data)
      return;

    std::memcpy(matrix_data, src, total * sizeof(element_type));
  }
}

template <avnd::tensor_port Field>
inline void matrix_to_tensor(void* matrix, Field& field)
{
  if(!matrix)
    return;

  matrix_lock lock(matrix);

  t_jit_matrix_info info;
  jit_object_method(matrix, _jit_sym_getinfo, &info);

  if(info.dimcount <= 0)
    return;

  const std::size_t rank = static_cast<std::size_t>(info.dimcount);
  if(rank > static_cast<std::size_t>(jitter_max_dimcount))
    return;

  std::size_t shape_buf[jitter_max_dimcount];
  std::size_t total = 1;
  for(std::size_t i = 0; i < rank; ++i)
  {
    shape_buf[i] = static_cast<std::size_t>(info.dim[i]);
    total *= shape_buf[i];
  }

  if(info.planecount != 1)
    return;

  if(total == 0)
    return;

  void* matrix_data = nullptr;
  jit_object_method(matrix, _jit_sym_getdata, &matrix_data);
  if(!matrix_data)
    return;

  using value_type = std::remove_cvref_t<decltype(Field::value)>;
  using element_type = avnd::tensor_element<value_type>;
  const std::size_t bytes = total * sizeof(element_type);

  if constexpr(avnd::view_tensor_like<value_type>)
  {
    auto scratch = std::make_shared<std::vector<element_type>>(total);
    std::memcpy(scratch->data(), matrix_data, bytes);
    avnd::set_view_buffer(
        field.value, scratch->data(),
        std::vector<std::size_t>(shape_buf, shape_buf + rank),
        std::shared_ptr<void>(scratch, scratch->data()));
  }
  else if constexpr(avnd::resizable_tensor_like<value_type>)
  {
    std::vector<std::size_t> shape_vec(shape_buf, shape_buf + rank);
    avnd::resize_to(field.value, shape_vec);
    auto* dst = avnd::data_of(field.value);
    if(dst)
      std::memcpy(dst, matrix_data, bytes);
  }
  else if constexpr(avnd::mutable_tensor_like<value_type>)
  {
    auto cur_shape = avnd::shape_of(field.value);
    std::size_t cur_total = 1;
    bool shape_ok = std::ranges::size(cur_shape) == rank;
    std::size_t i = 0;
    if(shape_ok)
    {
      for(auto extent : cur_shape)
      {
        if(i >= rank || static_cast<std::size_t>(extent) != shape_buf[i])
        {
          shape_ok = false;
          break;
        }
        cur_total *= static_cast<std::size_t>(extent);
        ++i;
      }
    }
    if(shape_ok && cur_total == total)
    {
      auto* dst = avnd::data_of(field.value);
      if(dst)
        std::memcpy(dst, matrix_data, bytes);
    }
  }
}

}
