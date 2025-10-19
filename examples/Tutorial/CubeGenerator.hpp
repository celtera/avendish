#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/geometry.hpp>

namespace examples
{

// Simple vec3 type
struct vec3
{
  float x, y, z;
};

struct CubeGenerator
{
  halp_meta(name, "Cube Generator");
  halp_meta(c_name, "avnd_cube");
  halp_meta(category, "Generator");
  halp_meta(author, "Avendish");
  halp_meta(description, "Generate a cube with customizable size and color");
  halp_meta(uuid, "ca657864-3086-4626-adc2-fd485f677601");

  struct
  {
    halp::hslider_f32<"Size", halp::range{0.1f, 10.f, 1.f}> size;
    halp::color_chooser<"Color"> color;
  } inputs;

  struct
  {
    struct
    {
      halp_meta(name, "Geometry");
      halp::position_normals_color_index_geometry mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;
  } outputs;

  std::vector<vec3> positions;
  std::vector<uint32_t> indices;
  std::vector<vec3> normals;
  std::vector<std::array<float, 4>> colors;

  void operator()()
  {
    const float s = inputs.size.value;
    const float r = inputs.color.value.r;
    const float g = inputs.color.value.g;
    const float b = inputs.color.value.b;
    const float a = inputs.color.value.a;

    //auto& vertices = outputs.mesh.vertices;
    //auto& indices = outputs.mesh.indices;
    //auto& normals = outputs.mesh.attributes.normals;
    //auto& colors = outputs.mesh.attributes.colors;

    // Clear previous data
    positions.clear();
    indices.clear();
    normals.clear();
    colors.clear();

    // Create cube vertices (8 corners)
    positions = {
        {-s, -s, -s}, // 0
        {s, -s, -s},  // 1
        {s, s, -s},   // 2
        {-s, s, -s},  // 3
        {-s, -s, s},  // 4
        {s, -s, s},   // 5
        {s, s, s},    // 6
        {-s, s, s}    // 7
    };

    // Cube indices (12 triangles, 2 per face)
    indices = {
               // Front face
               0, 1, 2, 0, 2, 3,
               // Back face
               5, 4, 7, 5, 7, 6,
               // Top face
               3, 2, 6, 3, 6, 7,
               // Bottom face
               4, 5, 1, 4, 1, 0,
               // Right face
               1, 5, 6, 1, 6, 2,
               // Left face
               4, 0, 3, 4, 3, 7};

    // Generate normals (simple per-vertex normals pointing outward)
    for(const auto& v : positions)
    {
      float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
      if(len > 0.0f)
      {
        normals.push_back({v.x / len, v.y / len, v.z / len});
      }
      else
      {
        normals.push_back({0.f, 1.f, 0.f});
      }
    }

    // Set color for all vertices
    for(size_t i = 0; i < positions.size(); ++i)
    {
      colors.push_back({r, g, b, a});
    }

    // Then setup our geometry buffers
    auto& mesh = outputs.geometry.mesh;
    mesh.buffers.position_buffer.data = reinterpret_cast<float*>(positions.data());
    mesh.buffers.position_buffer.size = positions.size();
    mesh.buffers.position_buffer.dirty = true;
    mesh.buffers.normal_buffer.data = reinterpret_cast<float*>(normals.data());
    mesh.buffers.normal_buffer.size = normals.size();
    mesh.buffers.normal_buffer.dirty = true;
    mesh.buffers.color_buffer.data = reinterpret_cast<float*>(colors.data());
    mesh.buffers.color_buffer.size = colors.size();
    mesh.buffers.color_buffer.dirty = true;
    mesh.buffers.index_buffer.data = indices.data();
    mesh.buffers.index_buffer.size = indices.size();
    mesh.buffers.index_buffer.dirty = true;

    mesh.vertices = 8;

    std::memset(outputs.geometry.transform, 0, sizeof(outputs.geometry.transform));
    outputs.geometry.transform[0] = 1.;
    outputs.geometry.transform[5] = 1.;
    outputs.geometry.transform[10] = 1.;
    outputs.geometry.transform[15] = 1.;

    outputs.geometry.dirty_mesh = true;
    outputs.geometry.dirty_transform = true;
  }
};

}
