#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Converts an Avendish geometry port (static or dynamic) into a Jitter
// t_jit_glchunk: an interleaved vertex matrix in the OpenGL Matrix Format
// (planes: 0-2 position, 3-4 texcoord, 5-7 normal, 8-11 color, 12 edge) plus an
// optional int32 index matrix and a drawing-primitive symbol.
//
// The gather logic mirrors binding/godot/geometry_node.hpp; the fill logic
// targets Jitter's chunk matrices instead of Godot PackedArrays.

#include <avnd/common/enums.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/port.hpp>
#include <halp/geometry.hpp>
#include <halp/polyfill.hpp>

#include <ext.h>
#include <jit.common.h>
// jit.gl.h is the umbrella that includes jit.gl.chunk.h (t_jit_glchunk) *before*
// jit.gl.ob3d.h, which is the only correct include order. Requires MAC_VERSION or
// WIN_VERSION (set by the Max build on its supported platforms).
#include <jit.gl.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace max::jitter
{

// Plane offsets in the OpenGL Matrix Format, indexed by Avendish attribute
// location (0 position, 1 texcoord, 2 color, 3 normal, 4 tangent).
struct gl_plane_layout
{
  int offset; // first plane index for this group, -1 if not representable
  int max_components;
};
inline constexpr gl_plane_layout gl_plane_for_location(int loc) noexcept
{
  switch(loc)
  {
    case 0: return {0, 3};  // position -> planes 0..2
    case 1: return {3, 2};  // texcoord -> planes 3..4
    case 3: return {5, 3};  // normal   -> planes 5..7
    case 2: return {8, 4};  // color    -> planes 8..11
    default: return {-1, 0}; // tangent / unknown: no OpenGL-matrix slot
  }
}

// Detect the location of a *static* geometry attribute struct (same scheme as
// the Godot binding): position=0, texcoord=1, color=2, normal=3, tangent=4.
template <typename Attr>
constexpr int static_attribute_location() noexcept
{
  if constexpr(requires { Attr::position; } || requires { Attr::positions; })
    return 0;
  else if constexpr(
      requires { Attr::texcoord; } || requires { Attr::tex_coord; }
      || requires { Attr::uv; })
    return 1;
  else if constexpr(requires { Attr::color; } || requires { Attr::colors; })
    return 2;
  else if constexpr(requires { Attr::normal; } || requires { Attr::normals; })
    return 3;
  else if constexpr(requires { Attr::tangent; } || requires { Attr::tangents; })
    return 4;
  else
    return -1;
}

// Normalised, backend-agnostic description of one vertex attribute stream.
struct gathered_attribute
{
  const float* data{};   // base pointer, already advanced by all byte offsets
  int stride_floats{};   // per-vertex stride, in floats
  int components{};      // floats per vertex (2,3,4)
  bool present{};
};

struct gathered_geometry
{
  int vertex_count{};
  std::array<gathered_attribute, 5> by_location{}; // indexed by location 0..4

  int index_count{};
  const void* index_data{};
  int index_width{};     // element size in bytes: 2 (uint16) or 4 (uint32)

  t_symbol* prim{};
};

// --- topology -> Jitter primitive symbol ------------------------------------
// Header-confirmed chunk primitives: "tri", "tri_strip", "quads", "quad_grid".
// The line/point/fan forms follow Jitter's documented names; validate in Max.
template <typename Geom>
inline t_symbol* topology_to_prim(const Geom& geom)
{
  static_constexpr auto m = AVND_ENUM_OR_TAG_MATCHER(
      topology,
      (gensym("tri"), triangle, triangles),
      (gensym("tri_strip"), triangle_strip, triangles_strip),
      (gensym("tri_fan"), triangle_fan),
      (gensym("line"), line, lines),
      (gensym("line_strip"), line_strip),
      (gensym("point"), point, points));
  return m(geom, gensym("tri"));
}

// --- gather: static geometry -------------------------------------------------
template <avnd::static_geometry_type Geom>
inline gathered_geometry gather_geometry(const Geom& geom)
{
  gathered_geometry out;
  out.prim = topology_to_prim(geom);

  if constexpr(requires { geom.vertices; })
    out.vertex_count = geom.vertices;
  if(out.vertex_count <= 0)
    return out;

  auto& mgeom = const_cast<Geom&>(geom);

  // Resolve binding strides once.
  std::array<int, 16> binding_stride_floats{};
  {
    int bind_idx = 0;
    avnd::for_each_field_ref(avnd::get_bindings(mgeom), [&](auto& bind) {
      if(bind_idx < (int)binding_stride_floats.size())
        binding_stride_floats[bind_idx] = bind.stride / sizeof(float);
      ++bind_idx;
    });
  }

  // For each attribute, resolve its buffer pointer + byte offset + stride.
  avnd::for_each_field_ref(avnd::get_attributes(mgeom), [&]<typename Attr>(Attr& attr) {
    constexpr int loc = static_attribute_location<Attr>();
    if constexpr(loc < 0 || loc > 4)
      return;
    else
    {
      const int binding_idx = [&] {
        if constexpr(requires { attr.binding; })
          return (int)attr.binding;
        else
          return 0;
      }();
      const int attr_offset = [&] {
        if constexpr(requires { attr.byte_offset; })
          return (int)attr.byte_offset;
        else if constexpr(requires { attr.offset; })
          return (int)attr.offset;
        else
          return 0;
      }();
      constexpr int components = [] {
        if constexpr(requires { typename Attr::datatype; })
          return (int)(sizeof(typename Attr::datatype) / sizeof(float));
        else
          return gl_plane_for_location(loc).max_components;
      }();

      // Find buffer pointer + byte offset by walking the input bindings.
      const float* buf_data = nullptr;
      int buf_byte_offset = 0;
      int input_idx = 0;
      avnd::for_each_field_ref(mgeom.input, [&]<typename Input>(Input& inp) {
        if(input_idx == binding_idx)
        {
          if constexpr(requires { inp.buffer(); })
          {
            constexpr auto buf_ptr = Input::buffer();
            auto& buf = mgeom.buffers.*buf_ptr;
            if constexpr(requires { buf.data; })
              buf_data = (const float*)buf.data;
            else if constexpr(requires { buf.elements; })
              buf_data = (const float*)buf.elements;
            else if constexpr(requires { buf.raw_data; })
              buf_data = (const float*)buf.raw_data;
          }
          if constexpr(requires { inp.byte_offset(); })
            buf_byte_offset = inp.byte_offset();
          else if constexpr(requires { inp.byte_offset; })
            buf_byte_offset = inp.byte_offset;
          else if constexpr(requires { inp.offset; })
            buf_byte_offset = inp.offset;
        }
        ++input_idx;
      });

      if(!buf_data)
        return;

      int stride_floats = (binding_idx < (int)binding_stride_floats.size())
                              ? binding_stride_floats[binding_idx]
                              : 0;
      if(stride_floats == 0)
        stride_floats = components;

      auto& slot = out.by_location[loc];
      slot.data = (const float*)((const char*)buf_data + buf_byte_offset + attr_offset);
      slot.stride_floats = stride_floats;
      slot.components = components;
      slot.present = true;
    }
  });

  // Index buffer.
  if constexpr(requires { geom.index; })
  {
    if constexpr(requires { decltype(geom.index)::buffer(); })
    {
      constexpr auto idx_buf_ptr = decltype(geom.index)::buffer();
      auto& idx_buf = mgeom.buffers.*idx_buf_ptr;
      out.index_count = [&] {
        if constexpr(requires { geom.indices; })
          return (int)geom.indices;
        else if constexpr(requires { idx_buf.element_count; })
          return (int)idx_buf.element_count;
        else
          return 0;
      }();
      if constexpr(requires { idx_buf.elements; })
      {
        out.index_data = idx_buf.elements;
        out.index_width = sizeof(std::remove_pointer_t<decltype(idx_buf.elements)>);
      }
    }
  }

  return out;
}

// --- gather: dynamic geometry ------------------------------------------------
template <avnd::dynamic_geometry_type Geom>
inline gathered_geometry gather_geometry(const Geom& geom)
{
  gathered_geometry out;
  out.prim = topology_to_prim(geom);

  if constexpr(requires { geom.vertices; })
    out.vertex_count = geom.vertices;
  if(out.vertex_count <= 0)
    return out;

  // format -> float component count
  static_constexpr auto fmt_components = AVND_ENUM_MATCHER(
      (4, float4, vec4), (3, float3, vec3), (2, float2, vec2), (1, float1));

  for(const auto& attr : geom.attributes)
  {
    const int loc = (int)attr.location;
    if(loc < 0 || loc > 4)
      continue;

    const int binding_idx = attr.binding;
    if(binding_idx < 0 || binding_idx >= (int)geom.input.size())
      continue;

    const auto& inp = geom.input[binding_idx];
    const int buf_idx = inp.buffer;
    if(buf_idx < 0 || buf_idx >= (int)geom.buffers.size())
      continue;

    const auto& buf = geom.buffers[buf_idx];
    const void* buf_data = nullptr;
    if constexpr(requires { buf.raw_data; })
      buf_data = buf.raw_data;
    else if constexpr(requires { buf.data; })
      buf_data = buf.data;
    if(!buf_data)
      continue;

    const int components = fmt_components(attr.format, gl_plane_for_location(loc).max_components);
    int stride_floats = (binding_idx < (int)geom.bindings.size())
                            ? geom.bindings[binding_idx].stride / (int)sizeof(float)
                            : 0;
    if(stride_floats == 0)
      stride_floats = components;

    const int attr_offset = (int)attr.byte_offset;
    const int buf_byte_offset = (int)inp.byte_offset;

    auto& slot = out.by_location[loc];
    slot.data = (const float*)((const char*)buf_data + buf_byte_offset + attr_offset);
    slot.stride_floats = stride_floats;
    slot.components = components;
    slot.present = true;
  }

  // Index buffer.
  if constexpr(requires { geom.index; })
  {
    if(geom.index.buffer >= 0 && geom.index.buffer < (int)geom.buffers.size())
    {
      const auto& idx_buf = geom.buffers[geom.index.buffer];
      out.index_count = [&] {
        if constexpr(requires { geom.indices; })
          return (int)geom.indices;
        else
          return 0;
      }();
      if constexpr(requires { idx_buf.raw_data; })
      {
        out.index_data = idx_buf.raw_data;
        // dynamic index format: uint16 vs uint32
        out.index_width = (geom.index.format == halp::index_format::uint16) ? 2 : 4;
      }
    }
  }

  return out;
}

// Plane count is positional: including a later group forces the earlier
// (possibly absent) groups' planes to exist as well.
inline int compute_planes(const gathered_geometry& g)
{
  int planes = 3;
  if(g.by_location[1].present)
    planes = std::max(planes, 5);
  if(g.by_location[3].present)
    planes = std::max(planes, 8);
  if(g.by_location[2].present)
    planes = std::max(planes, 12);
  return planes;
}

// Mask the groups we did not fill so the renderer falls back to ob3d state.
inline long compute_flags(const gathered_geometry& g, int planes)
{
  long flags = JIT_GL_CHUNK_IGNORE_EDGES;
  if(planes >= 5 && !g.by_location[1].present)
    flags |= JIT_GL_CHUNK_IGNORE_TEXTURES;
  if(planes >= 8 && !g.by_location[3].present)
    flags |= JIT_GL_CHUNK_IGNORE_NORMALS;
  if(planes >= 12 && !g.by_location[2].present)
    flags |= JIT_GL_CHUNK_IGNORE_COLORS;
  return flags;
}

// Fill an already-allocated chunk's prim, flags and matrix data from g.
inline void fill_chunk(t_jit_glchunk* chunk, const gathered_geometry& g, int planes)
{
  chunk->prim = g.prim;
  chunk->m_flags = compute_flags(g, planes);

  // Fill the interleaved vertex matrix.
  {
    t_jit_matrix_info info;
    jit_object_method(chunk->m_vertex, _jit_sym_getinfo, &info);
    char* base = nullptr;
    jit_object_method(chunk->m_vertex, _jit_sym_getdata, &base);
    if(base)
    {
      const long cell_stride = info.dimstride[0]; // bytes between vertices
      for(int i = 0; i < g.vertex_count; ++i)
      {
        float* cell = (float*)(base + (std::ptrdiff_t)i * cell_stride);
        std::memset(cell, 0, planes * sizeof(float));

        for(int loc = 0; loc <= 3; ++loc)
        {
          const auto& a = g.by_location[loc];
          if(!a.present)
            continue;
          const auto [poff, pmax] = gl_plane_for_location(loc);
          if(poff < 0)
            continue;
          const int n = std::min(a.components, pmax);
          const float* src = a.data + (std::ptrdiff_t)i * a.stride_floats;
          for(int c = 0; c < n; ++c)
            cell[poff + c] = src[c];
          // RGB colour without alpha -> opaque.
          if(loc == 2 && n == 3 && planes >= 12)
            cell[poff + 3] = 1.f;
        }
      }
    }
  }

  // Fill the index matrix (widen to int32).
  if(g.index_count > 0 && g.index_data && chunk->m_index)
  {
    t_jit_matrix_info iinfo;
    jit_object_method(chunk->m_index, _jit_sym_getinfo, &iinfo);
    char* ibase = nullptr;
    jit_object_method(chunk->m_index, _jit_sym_getdata, &ibase);
    if(ibase)
    {
      const long istride = iinfo.dimstride[0];
      for(int i = 0; i < g.index_count; ++i)
      {
        int32_t v = (g.index_width == 2)
                        ? (int32_t)((const uint16_t*)g.index_data)[i]
                        : (int32_t)((const uint32_t*)g.index_data)[i];
        *(int32_t*)(ibase + (std::ptrdiff_t)i * istride) = v;
      }
    }
  }
}

// Allocate a fresh chunk + fill it.
inline t_jit_glchunk* build_chunk(const gathered_geometry& g)
{
  if(g.vertex_count <= 0)
    return nullptr;
  const int planes = compute_planes(g);
  t_jit_glchunk* chunk
      = jit_glchunk_new(g.prim, planes, g.vertex_count, g.index_count);
  if(!chunk)
    return nullptr;
  fill_chunk(chunk, g, planes);
  return chunk;
}

// Reuse `chunk` in place when its matrices already match the new geometry's
// dimensions (only the data changed); otherwise free + reallocate. Avoids the
// per-update jit_matrix (re)allocation when the vertex/index counts are stable.
inline t_jit_glchunk* refresh_chunk(t_jit_glchunk* chunk, const gathered_geometry& g)
{
  if(g.vertex_count <= 0)
  {
    if(chunk)
      jit_glchunk_delete(chunk);
    return nullptr;
  }

  const int planes = compute_planes(g);
  if(chunk && chunk->m_vertex)
  {
    t_jit_matrix_info vi;
    jit_object_method(chunk->m_vertex, _jit_sym_getinfo, &vi);

    bool idx_ok;
    if(g.index_count > 0)
    {
      if(chunk->m_index)
      {
        t_jit_matrix_info ii;
        jit_object_method(chunk->m_index, _jit_sym_getinfo, &ii);
        idx_ok = (ii.dim[0] == g.index_count);
      }
      else
        idx_ok = false;
    }
    else
      idx_ok = (chunk->m_index == nullptr);

    if(vi.planecount == planes && vi.dim[0] == g.vertex_count && idx_ok)
    {
      fill_chunk(chunk, g, planes);
      return chunk;
    }
  }

  if(chunk)
    jit_glchunk_delete(chunk);
  return build_chunk(g);
}

// Axis-aligned bounding box of the position stream -> out = {minx,miny,minz,
// maxx,maxy,maxz}. Returns false if there is no usable position data.
inline bool geometry_bounds(const gathered_geometry& g, float out[6])
{
  const auto& p = g.by_location[0];
  if(!p.present || !p.data || g.vertex_count <= 0)
    return false;

  const int nc = std::min(p.components, 3);
  float mn[3] = {0, 0, 0}, mx[3] = {0, 0, 0};
  for(int c = 0; c < nc; ++c)
    mn[c] = mx[c] = p.data[c];

  for(int i = 1; i < g.vertex_count; ++i)
  {
    const float* v = p.data + (std::ptrdiff_t)i * p.stride_floats;
    for(int c = 0; c < nc; ++c)
    {
      mn[c] = std::min(mn[c], v[c]);
      mx[c] = std::max(mx[c], v[c]);
    }
  }

  out[0] = mn[0]; out[1] = mn[1]; out[2] = mn[2];
  out[3] = mx[0]; out[4] = mx[1]; out[5] = mx[2];
  return true;
}

template <typename Geom>
inline t_jit_glchunk* geometry_to_chunk(const Geom& geom)
{
  return build_chunk(gather_geometry(geom));
}

}
