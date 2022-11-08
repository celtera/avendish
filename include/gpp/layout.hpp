#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/for_nth.hpp>
#include <avnd/common/member_reflection.hpp>
#include <boost/pfr/core.hpp>

/*
namespace gpp
{

// basic enum and type definition
enum class layouts
{
  std140
};
enum class samplers
{
  sampler1D,
  sampler2D,
  sampler3D,
  samplerCube,
  sampler2DRect
};
enum class buffer_type
{
  immutable_buffer,
  static_buffer,
  dynamic_buffer
};
enum class buffer_usage
{
  vertex,
  index,
  uniform,
  storage
};
enum class binding_stage
{
  vertex,
  fragment
};

enum class default_attributes
{
  position,
  normal,
  tangent,
  texcoord,
  color
};
enum class default_uniforms
{
  // Render-level default uniforms
  clip_space_matrix // float[16]
  , texcoord_adjust // float[2]
  , render_size // float[2]

  // Process-level default uniforms
  , time // float
  , time_delta // float
  , progress // float
  , pass_index // int
  , frame_index // int
  , date // float[4]
  , mouse // float[4]
  , channel_time // float[4]
  , sample_rate // float
};

template <typename T>
consteval int binding()
{
  return T::binding();
}
template <typename T>
consteval int std140_size()
{
  int sz = 0;
  constexpr int field_count = pfr::tuple_size_v<T>;
  auto func = [&](auto field)
  {
    switch (sizeof(field.value))
    {
      case 4:
        sz += 4;
        break;
      case 8:
        if (sz % 8 != 0)
          sz += 4;
        sz += 8;
        break;
      case 12:
        while (sz % 16 != 0)
          sz += 4;
        sz += 12;
        break;
      case 16:
        while (sz % 16 != 0)
          sz += 4;
        sz += 16;
        break;
    }
  };

  if constexpr (field_count > 0)
  {
    [&func]<typename K, K... Index>(std::integer_sequence<K, Index...>)
    {
      constexpr T t{};
      (func(pfr::get<Index, T>(t)), ...);
    }
    (std::make_index_sequence<field_count>{});
  }
  return sz;
}

}

*/

namespace gpp
{

template <typename T, std::size_t Count>
consteval int std140_offset_impl()
{
  int sz = 0;
  auto func = [&](auto field) {
    switch(sizeof(field.value))
    {
      case 4:
        sz += 4;
        break;
      case 8:
        if(sz % 8 != 0)
          sz += 4;
        sz += 8;
        break;
      case 12:
        while(sz % 16 != 0)
          sz += 4;
        sz += 12;
        break;
      case 16:
        while(sz % 16 != 0)
          sz += 4;
        sz += 16;
        break;
    }
  };

  if constexpr(Count > 0)
  {
    [&func]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
      constexpr T t{};
      (func(boost::pfr::get<Index>(t)), ...);
        }(std::make_index_sequence<Count>{});
  }
  return sz;
}

template <auto F>
consteval int std140_offset()
{
  using ubo_type = typename avnd::member_reflection<F>::class_type;
  constexpr ubo_type ubo{};
  constexpr int field_offset = avnd::index_in_struct(ubo, F);
  return std140_offset_impl<ubo_type, field_offset>();
}

template <typename T>
consteval int std140_size()
{
  return std140_offset_impl<T, boost::pfr::tuple_size_v<T>>();
}

}
