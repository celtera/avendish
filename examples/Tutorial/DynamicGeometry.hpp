#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/geometry.hpp>
#include <halp/meta.hpp>

#include <cstring>
#include <vector>

namespace examples
{
/**
 * Example of an object producing a run-time defined geometry:
 * a unit quad with position, normal, texcoord and color attributes
 * identified through semantic tagging, and an indexed triangle list.
 */
struct DynamicGeometryExample
{
  halp_meta(name, "Dynamic geometry")
  halp_meta(c_name, "avnd_dynamic_geometry")
  halp_meta(category, "Visuals/3D")
  halp_meta(uuid, "33b8f6f3-5b9f-4f9c-94a3-67de2eae3e44")

  struct
  {
  } inputs;

  struct
  {
    halp::dynamic_geometry geometry;
  } outputs;

  // clang-format off
  static constexpr float vertex_data[] = {
      // positions (float3 * 4)
      0, 0, 0,  1, 0, 0,  1, 1, 0,  0, 1, 0,
      // normals (float3 * 4)
      0, 0, 1,  0, 0, 1,  0, 0, 1,  0, 0, 1,
      // texcoords (float2 * 4)
      0, 0,  1, 0,  1, 1,  0, 1,
      // colors (float4 * 4)
      1, 0, 0, 1,  0, 1, 0, 1,  0, 0, 1, 1,  1, 1, 1, 0.5f};
  // clang-format on
  static constexpr uint32_t index_data[] = {0, 1, 2, 0, 2, 3};

  std::vector<float> vertex_buffer{std::begin(vertex_data), std::end(vertex_data)};
  std::vector<uint32_t> index_buffer{std::begin(index_data), std::end(index_data)};

  DynamicGeometryExample()
  {
    auto& geom = outputs.geometry;
    geom.buffers = {
        {.raw_data = vertex_buffer.data(),
         .byte_size = int64_t(vertex_buffer.size() * sizeof(float)),
         .dirty = true},
        {.raw_data = index_buffer.data(),
         .byte_size = int64_t(index_buffer.size() * sizeof(uint32_t)),
         .dirty = true},
    };

    geom.bindings = {
        {.stride = 3 * sizeof(float), .step_rate = 1},
        {.stride = 3 * sizeof(float), .step_rate = 1},
        {.stride = 2 * sizeof(float), .step_rate = 1},
        {.stride = 4 * sizeof(float), .step_rate = 1},
    };

    geom.attributes = {
        {.binding = 0, .semantic = halp::attribute_semantic::position,
         .format = halp::attribute_format::float3},
        {.binding = 1, .semantic = halp::attribute_semantic::normal,
         .format = halp::attribute_format::float3},
        {.binding = 2, .semantic = halp::attribute_semantic::texcoord0,
         .format = halp::attribute_format::float2},
        {.binding = 3, .semantic = halp::attribute_semantic::color0,
         .format = halp::attribute_format::float4},
    };

    // One region of the same buffer per binding
    geom.input = {
        {.buffer = 0, .byte_offset = 0},
        {.buffer = 0, .byte_offset = 12 * sizeof(float)},
        {.buffer = 0, .byte_offset = 24 * sizeof(float)},
        {.buffer = 0, .byte_offset = 32 * sizeof(float)},
    };

    geom.index = {.buffer = 1, .byte_offset = 0, .format = halp::index_format::uint32};

    geom.vertices = 4;
    geom.indices = 6;
    geom.topology = halp::primitive_topology::triangles;
  }

  struct
  {
    bool dirty = true;
  } state;

  void operator()()
  {
    // Static content: only upload once
    if(state.dirty)
    {
      for(auto& buf : outputs.geometry.buffers)
        buf.dirty = true;
      state.dirty = false;
    }
  }
};
}
