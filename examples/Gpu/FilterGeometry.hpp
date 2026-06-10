#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <avnd/concepts/gfx.hpp>
#include <halp/geometry.hpp>

#include <algorithm>
#include <cassert>
#include <span>

namespace examples
{
static_assert(avnd::geometry_port<halp::dynamic_geometry>);

struct FilterGeometry
{
  static consteval auto name() { return "Filter Geometry"; }
  static consteval auto c_name() { return "avnd_filter_geometry"; }
  static consteval auto uuid() { return "4f493663-3739-43df-94b5-20a31c4dc8aa"; }

  struct
  {
    halp::dynamic_geometry geometry;
  } inputs;

  struct
  {
    halp::dynamic_geometry geometry;
  } outputs;

  void operator()()
  {
    outputs.geometry = inputs.geometry;
    if(outputs.geometry.buffers.empty())
      return;

    // Find the position attribute through its semantic
    auto& attr = outputs.geometry.attributes;
    auto it = std::find_if(attr.begin(), attr.end(), [](auto& a) {
      return a.semantic == halp::attribute_semantic::position;
    });
    if(it == attr.end())
      return;

    const int binding = it->binding;
    auto& ins = outputs.geometry.input;
    assert(binding >= 0);
    assert(binding < std::ssize(ins));

    const int buffer = ins[binding].buffer;

    auto& bufs = outputs.geometry.buffers;
    assert(buffer >= 0);
    assert(buffer < std::ssize(bufs));

    auto& buf = bufs[buffer];
    if(!buf.raw_data)
      return;

    // Cheat a bit for now... and assume that positions aren't interleaved
    // and that we have float[3]s
    using type = float[3];
    std::span<type> vertices(
        (type*)((char*)buf.raw_data + ins[binding].byte_offset + it->byte_offset),
        outputs.geometry.vertices);

    for(float(&v)[3] : vertices)
    {
      v[0] += 0.01;
      v[1] += 0.01;
      v[2] += 0.01;
    }

    buf.dirty = true;
  }
};
}
