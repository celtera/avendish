#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <avnd/concepts/gfx.hpp>

#include <cstdlib>
#include <cstring>
#include <random>
#include <span>

namespace examples
{
struct dynamic_geometry
{
  struct buffer
  {
    void* data{};
    int size{};
    bool dirty{};
  };

  struct binding
  {
    int stride{};
    enum
    {
      per_vertex,
      per_instance
    } classification{};
    int step_rate{};
  };

  struct attribute
  {
    int binding = 0;
    enum : uint32_t
    {
      position,
      tex_coord,
      color,
      normal,
      tangent
    } location{};
    enum
    {
      float4,
      float3,
      float2,
      float1,
      uint4,
      uint2,
      uint1
    } format{};
    int32_t offset{};
  };

  struct input
  {
    int buffer{}; // Index of the buffer to use
    int offset{};
  };

  std::vector<buffer> buffers;
  std::vector<binding> bindings;
  std::vector<attribute> attributes;
  std::vector<input> input;
  int vertices = 0;
  enum
  {
    triangles,
    triangle_strip,
    triangle_fan,
    lines,
    line_strip,
    points
  } topology;
  enum
  {
    none,
    front,
    back
  } cull_mode;
  enum
  {
    counter_clockwise,
    clockwise
  } front_face;

  struct
  {
    int buffer{-1};
    int offset{};
    enum
    {
      uint16,
      uint32
    } format{};
  } index;

  bool dirty{};
};
static_assert(avnd::geometry_port<dynamic_geometry>);

struct FilterGeometry
{
  static consteval auto name() { return "Filter Geometry"; }
  static consteval auto c_name() { return "avnd_filter_geometry"; }
  static consteval auto uuid() { return "4f493663-3739-43df-94b5-20a31c4dc8aa"; }

  struct
  {
    dynamic_geometry geometry;
  } inputs;

  struct
  {
    dynamic_geometry geometry;
  } outputs;

  FilterGeometry() { }

  ~FilterGeometry()
  {
    //delete outputs.geometry.buffers.main_buffer.data;
  }

  void operator()()
  {
    outputs.geometry = inputs.geometry;
    outputs.geometry.dirty = true;
    if(outputs.geometry.buffers.empty())
      return;

    // Find the position attribute:
    auto& attr = outputs.geometry.attributes;
    auto it = std::find_if(
        attr.begin(), attr.end(), [](auto& a) { return a.location == 0; });
    if(it == attr.end())
      return;

    const int binding = it->binding;
    auto& ins = outputs.geometry.input;
    assert(binding >= 0);
    assert(binding < ins.size());

    const int buffer = ins[binding].buffer;

    auto& bufs = outputs.geometry.buffers;
    assert(buffer >= 0);
    assert(buffer < bufs.size());

    auto& buf = bufs[buffer];

    // Cheat a bit for now... and assume that position comes first,
    // and that things aren't interleaved,
    // and that we have float[3]s, ...
    using type = float[3];
    std::span<type> vertices((type*)buf.data, outputs.geometry.vertices);

    for(float(&v)[3] : vertices)
    {
      v[0] += 0.01;
      v[1] += 0.01;
      v[2] += 0.01;
    }

    //auto b = (float*) outputs.geometry.buffers[0].data;
    //for(int i = 0; i < 16; i++)
    //{
    //  b[i] += 0.01;
    //}
    outputs.geometry.buffers[0].dirty = true;
  }
};
}
