#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace examples::tests
{
struct TestGeometryStaticGenerator
{
  halp_meta(name, "Test geometry (CPU static) generator")
  halp_meta(c_name, "avnd_test_geom_static")
  halp_meta(category, "Tests/Geometry")
  halp_meta(uuid, "38f2c5d4-dbe4-48d1-b1fb-df1f2b078cc3")

  struct { halp::hslider_f32<"Size", halp::range{0.1f, 4.f, 1.f}> size; } inputs;
  struct
  {
    struct
    {
      halp_meta(name, "Geometry")
      halp::position_normals_color_index_geometry mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;
  } outputs;

  struct vec3 { float x, y, z; };
  std::vector<vec3> positions, normals;
  std::vector<std::array<float, 4>> colors;
  std::vector<uint32_t> indices;

  void operator()()
  {
    const float s = inputs.size.value;
    positions = {{-s, -s, 0}, {s, -s, 0}, {0, s, 0}};
    normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
    colors = {{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}};
    indices = {0, 1, 2};

    auto& mesh = outputs.geometry.mesh;
    mesh.buffers.position_buffer.elements = reinterpret_cast<float*>(positions.data());
    mesh.buffers.position_buffer.element_count = positions.size();
    mesh.buffers.position_buffer.dirty = true;
    mesh.buffers.normal_buffer.elements = reinterpret_cast<float*>(normals.data());
    mesh.buffers.normal_buffer.element_count = normals.size();
    mesh.buffers.normal_buffer.dirty = true;
    mesh.buffers.color_buffer.elements = reinterpret_cast<float*>(colors.data());
    mesh.buffers.color_buffer.element_count = colors.size();
    mesh.buffers.color_buffer.dirty = true;
    mesh.buffers.index_buffer.elements = indices.data();
    mesh.buffers.index_buffer.element_count = indices.size();
    mesh.buffers.index_buffer.dirty = true;
    mesh.vertices = 3;
    outputs.geometry.dirty_mesh = true;
  }
};
}
