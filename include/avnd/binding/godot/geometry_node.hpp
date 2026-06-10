#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/godot/node.hpp>
#include <avnd/common/enums.hpp>
#include <avnd/introspection/port.hpp>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <cstdint>

namespace godot_binding
{

/// Map avendish topology to Godot primitive types.
/// Handles both compile-time tags and runtime `topology` enum members
/// (e.g. halp::dynamic_geometry).
template <typename Geom>
constexpr godot::Mesh::PrimitiveType godot_primitive_type(const Geom& g)
{
  static_constexpr auto m = AVND_ENUM_OR_TAG_MATCHER(
      topology,
      (godot::Mesh::PRIMITIVE_TRIANGLES, triangle, triangles),
      (godot::Mesh::PRIMITIVE_TRIANGLE_STRIP, triangle_strip, triangles_strip),
      (godot::Mesh::PRIMITIVE_LINES, line, lines),
      (godot::Mesh::PRIMITIVE_LINE_STRIP, line_strip),
      (godot::Mesh::PRIMITIVE_POINTS, point, points));
  return m(g, godot::Mesh::PRIMITIVE_TRIANGLES);
}

/// Detect the attribute location from an avendish attribute struct.
/// Uses the same enum-tag introspection as the ossia binding.
template <typename Attr>
constexpr int attribute_location()
{
  if constexpr(requires { Attr::position; } || requires { Attr::positions; })
    return 0; // position
  else if constexpr(
      requires { Attr::texcoord; } || requires { Attr::tex_coord; }
      || requires { Attr::uv; })
    return 1; // texcoord
  else if constexpr(requires { Attr::color; } || requires { Attr::colors; })
    return 2; // color
  else if constexpr(requires { Attr::normal; } || requires { Attr::normals; })
    return 3; // normal
  else if constexpr(requires { Attr::tangent; } || requires { Attr::tangents; })
    return 4; // tangent
  else
    return -1;
}

/// Map a compile-time attribute location (or a legacy runtime `location`
/// member; both use position = 0, texcoord, color, normal, tangent) to the
/// matching Godot mesh array, or -1 to skip the attribute.
constexpr int mesh_array_for_location(int location)
{
  switch(location)
  {
    case 0:
      return godot::Mesh::ARRAY_VERTEX;
    case 1:
      return godot::Mesh::ARRAY_TEX_UV;
    case 2:
      return godot::Mesh::ARRAY_COLOR;
    case 3:
      return godot::Mesh::ARRAY_NORMAL;
    case 4:
      return godot::Mesh::ARRAY_TANGENT;
    default:
      return -1;
  }
}

/// Map a halp::attribute_semantic-compatible numeric value to the matching
/// Godot mesh array. Semantics without a fixed Godot slot return -1 so that
/// the caller can assign them to ARRAY_CUSTOM0..3.
constexpr int mesh_array_for_semantic(uint32_t semantic)
{
  switch(semantic)
  {
    case 0: // position
      return godot::Mesh::ARRAY_VERTEX;
    case 1: // normal
      return godot::Mesh::ARRAY_NORMAL;
    case 2: // tangent
      return godot::Mesh::ARRAY_TANGENT;
    case 100: // texcoord0
      return godot::Mesh::ARRAY_TEX_UV;
    case 101: // texcoord1
      return godot::Mesh::ARRAY_TEX_UV2;
    case 200: // color0
      return godot::Mesh::ARRAY_COLOR;
    case 300: // joints0
      return godot::Mesh::ARRAY_BONES;
    case 400: // weights0
      return godot::Mesh::ARRAY_WEIGHTS;
    default:
      return -1;
  }
}

/// Number of 32-bit float components described by a runtime attribute format
/// enum (e.g. halp::attribute_format); -1 if the format is not float-based.
template <typename Fmt>
constexpr int format_float_components(Fmt fmt)
{
  static_constexpr auto m = AVND_ENUM_MATCHER(
      (4, float4, fp4, Float4, vec4), (3, float3, fp3, Float3, vec3),
      (2, float2, fp2, Float2, vec2), (1, float1, fp1, Float1));
  return m(fmt, -1);
}

/// Whether a runtime attribute format enum describes four 32-bit integers
/// (the layout Godot expects for ARRAY_BONES).
template <typename Fmt>
constexpr bool format_is_int4(Fmt fmt)
{
  static_constexpr auto m = AVND_ENUM_MATCHER(
      (true, uint4, unsigned4, u4, uvec4, sint4, signed4, i4, s4, ivec4));
  return m(fmt, false);
}

/// Extract geometry data from an avendish geometry port and build
/// a Godot Array suitable for ArrayMesh::add_surface_from_arrays.
///
/// Strategy: walk the geometry's attributes and input bindings; identify each
/// attribute either through a runtime `semantic` member (halp::dynamic_geometry),
/// a legacy runtime `location` member, or compile-time tags (static geometry
/// types). Read the raw data from the corresponding buffer + offset.
///
/// Attributes without a fixed Godot slot are packed into ARRAY_CUSTOM0..3 in
/// declaration order; the matching format flags to pass to
/// add_surface_from_arrays are accumulated in custom_format_flags.
template <typename Geom>
godot::Array build_mesh_arrays(const Geom& geom, uint64_t& custom_format_flags)
{
  godot::Array arrays;
  arrays.resize(godot::Mesh::ARRAY_MAX);

  const int vertex_count = [&] {
    if constexpr(requires { geom.vertices; })
      return geom.vertices;
    else
      return 0;
  }();

  if(vertex_count <= 0)
    return arrays;

  // Walk attributes and extract data based on semantic / location
  int next_custom = 0;
  auto&& attributes
      = avnd::get_attributes(const_cast<std::decay_t<decltype(geom)>&>(geom));
  avnd::for_each_field_ref(
      attributes,
      [&]<typename Attr>(Attr& attr) {
    // Determine which Godot mesh array this attribute feeds.
    // A runtime semantic member (e.g. halp::dynamic_geometry) takes
    // precedence, then a legacy runtime location member, then
    // compile-time tags.
    int target = -1;
    if constexpr(requires { attr.semantic; })
    {
      target = mesh_array_for_semantic(static_cast<uint32_t>(attr.semantic));
      if(target < 0)
        target = godot::Mesh::ARRAY_CUSTOM0; // actual slot assigned below
    }
    else if constexpr(requires { attr.location; })
    {
      target = mesh_array_for_location(static_cast<int>(attr.location));
    }
    else
    {
      target = mesh_array_for_location(attribute_location<Attr>());
    }

    if(target < 0)
      return;

    // Determine which buffer and offset this attribute uses
    // by looking at the binding index and matching to the input struct
    const int binding_idx = [&] {
      if constexpr(requires { attr.binding; })
        return attr.binding;
      else
        return 0;
    }();

    const int attr_offset = [&] {
      if constexpr(requires { attr.offset; })
        return attr.offset;
      else if constexpr(requires { attr.byte_offset; })
        return attr.byte_offset;
      else
        return 0;
    }();

    // Find the buffer pointer and byte offset for this input binding
    const float* buf_data = nullptr;
    int buf_byte_offset = 0;

    if constexpr(requires {
                   geom.input.size();
                   geom.buffers.size();
                 })
    {
      // Runtime input & buffer descriptions (e.g. halp::dynamic_geometry):
      // the input refers to its buffer through an index
      auto& self = const_cast<std::decay_t<decltype(geom)>&>(geom);
      if(binding_idx >= 0 && binding_idx < int(self.input.size()))
      {
        auto& inp = self.input[binding_idx];
        if(inp.buffer >= 0 && inp.buffer < int(self.buffers.size()))
        {
          auto& buf = self.buffers[inp.buffer];
          if constexpr(requires { buf.data; })
            buf_data = buf.data;
          else if constexpr(requires { buf.elements; })
            buf_data = buf.elements;
          else if constexpr(requires { buf.raw_data; })
            buf_data = static_cast<const float*>(buf.raw_data);
        }
        buf_byte_offset = inp.byte_offset;
      }
    }
    else
    {
      int input_idx = 0;
      avnd::for_each_field_ref(
          const_cast<std::decay_t<decltype(geom)>&>(geom).input,
          [&]<typename Input>(Input& inp) {
        if(input_idx == binding_idx)
        {
          // Get the buffer via the input's buffer() pointer-to-member
          if constexpr(requires { inp.buffer(); })
          {
            constexpr auto buf_ptr = Input::buffer();
            auto& buf
                = const_cast<std::decay_t<decltype(geom)>&>(geom).buffers.*buf_ptr;

            if constexpr(requires { buf.data; })
              buf_data = buf.data;
            else if constexpr(requires { buf.elements; })
              buf_data = buf.elements;
            else if constexpr(requires { buf.raw_data; })
              buf_data = static_cast<const float*>(buf.raw_data);
          }

          if constexpr(requires { inp.offset; })
            buf_byte_offset = inp.offset;
          else if constexpr(requires { inp.byte_offset(); })
            buf_byte_offset = inp.byte_offset();
          else if constexpr(requires { inp.byte_offset; })
            buf_byte_offset = inp.byte_offset;
        }
        ++input_idx;
      });
    }

    if(!buf_data)
      return;

    // Offset into the buffer (in floats)
    const float* data_start
        = reinterpret_cast<const float*>(
              reinterpret_cast<const char*>(buf_data) + buf_byte_offset
              + attr_offset);

    // Determine stride (in floats)
    int stride_floats = 0;
    {
      int bind_idx = 0;
      auto&& bindings
          = avnd::get_bindings(const_cast<std::decay_t<decltype(geom)>&>(geom));
      avnd::for_each_field_ref(
          bindings,
          [&]<typename Bind>(Bind& bind) {
        if(bind_idx == binding_idx)
        {
          stride_floats = bind.stride / sizeof(float);
        }
        ++bind_idx;
      });
    }

    // Number of float components of the attribute data: from the static
    // datatype if available, else from a runtime format member.
    // 0 means unknown (assume the usual layout for the target array),
    // -1 means an explicitly non-float format which we cannot copy.
    int components = 0;
    if constexpr(requires { typename Attr::datatype; })
      components = sizeof(typename Attr::datatype) / sizeof(float);
    else if constexpr(requires { attr.format; })
      components = format_float_components(attr.format);

    switch(target)
    {
      case godot::Mesh::ARRAY_VERTEX:
      case godot::Mesh::ARRAY_NORMAL: { // float3
        if(components == 0)
          components = 3;
        if(components < 3)
          return;
        if(stride_floats == 0)
          stride_floats = components;
        godot::PackedVector3Array vecs;
        vecs.resize(vertex_count);
        auto* dst = vecs.ptrw();
        for(int i = 0; i < vertex_count; ++i)
        {
          const float* v = data_start + i * stride_floats;
          dst[i] = godot::Vector3(v[0], v[1], v[2]);
        }
        arrays[target] = vecs;
        break;
      }

      case godot::Mesh::ARRAY_TEX_UV:
      case godot::Mesh::ARRAY_TEX_UV2: { // float2
        if(components == 0)
          components = 2;
        if(components < 2)
          return;
        if(stride_floats == 0)
          stride_floats = components;
        godot::PackedVector2Array uvs;
        uvs.resize(vertex_count);
        auto* dst = uvs.ptrw();
        for(int i = 0; i < vertex_count; ++i)
        {
          const float* v = data_start + i * stride_floats;
          dst[i] = godot::Vector2(v[0], v[1]);
        }
        arrays[target] = uvs;
        break;
      }

      case godot::Mesh::ARRAY_COLOR: { // float3 or float4
        if(components == 0)
          components = 3;
        if(components < 3)
          return;
        if(stride_floats == 0)
          stride_floats = components;
        godot::PackedColorArray colors;
        colors.resize(vertex_count);
        auto* dst = colors.ptrw();
        for(int i = 0; i < vertex_count; ++i)
        {
          const float* v = data_start + i * stride_floats;
          if(components >= 4)
            dst[i] = godot::Color(v[0], v[1], v[2], v[3]);
          else
            dst[i] = godot::Color(v[0], v[1], v[2], 1.0f);
        }
        arrays[godot::Mesh::ARRAY_COLOR] = colors;
        break;
      }

      case godot::Mesh::ARRAY_TANGENT: { // 4 floats per vertex (xyz + w)
        if(components == 0)
          components = 4;
        if(components < 3)
          return;
        if(stride_floats == 0)
          stride_floats = components;
        godot::PackedFloat32Array tangents;
        tangents.resize(vertex_count * 4);
        auto* dst = tangents.ptrw();
        for(int i = 0; i < vertex_count; ++i)
        {
          const float* v = data_start + i * stride_floats;
          dst[i * 4 + 0] = v[0];
          dst[i * 4 + 1] = v[1];
          dst[i * 4 + 2] = v[2];
          dst[i * 4 + 3] = components >= 4 ? v[3] : 1.0f;
        }
        arrays[godot::Mesh::ARRAY_TANGENT] = tangents;
        break;
      }

      case godot::Mesh::ARRAY_WEIGHTS: { // 4 floats per vertex
        if(components == 0)
          components = 4;
        if(components < 4)
          return;
        if(stride_floats == 0)
          stride_floats = components;
        godot::PackedFloat32Array weights;
        weights.resize(vertex_count * 4);
        auto* dst = weights.ptrw();
        for(int i = 0; i < vertex_count; ++i)
        {
          const float* v = data_start + i * stride_floats;
          for(int c = 0; c < 4; ++c)
            dst[i * 4 + c] = v[c];
        }
        arrays[godot::Mesh::ARRAY_WEIGHTS] = weights;
        break;
      }

      case godot::Mesh::ARRAY_BONES: { // 4 32-bit ints per vertex
        bool int_data = false;
        if constexpr(requires { attr.format; })
          int_data = format_is_int4(attr.format);
        if(!int_data && components < 4)
          return;
        if(stride_floats == 0)
          stride_floats = 4;
        godot::PackedInt32Array bones;
        bones.resize(vertex_count * 4);
        auto* dst = bones.ptrw();
        for(int i = 0; i < vertex_count; ++i)
        {
          const float* v = data_start + i * stride_floats;
          if(int_data)
          {
            auto* iv = reinterpret_cast<const int32_t*>(v);
            for(int c = 0; c < 4; ++c)
              dst[i * 4 + c] = iv[c];
          }
          else
          {
            for(int c = 0; c < 4; ++c)
              dst[i * 4 + c] = int32_t(v[c]);
          }
        }
        arrays[godot::Mesh::ARRAY_BONES] = bones;
        break;
      }

      case godot::Mesh::ARRAY_CUSTOM0: {
        // Semantics without a dedicated Godot slot: pack the raw floats in
        // the next free ARRAY_CUSTOM0..3 slot, skip when all are taken.
        // Only float1..4 formats can be forwarded as-is.
        if(components < 1 || components > 4)
          return;
        if(next_custom >= 4)
          return;
        if(stride_floats == 0)
          stride_floats = components;
        godot::PackedFloat32Array data;
        data.resize(vertex_count * components);
        auto* dst = data.ptrw();
        for(int i = 0; i < vertex_count; ++i)
        {
          const float* v = data_start + i * stride_floats;
          for(int c = 0; c < components; ++c)
            dst[i * components + c] = v[c];
        }
        const int slot = next_custom++;
        arrays[godot::Mesh::ARRAY_CUSTOM0 + slot] = data;

        // ARRAY_CUSTOM_R_FLOAT..ARRAY_CUSTOM_RGBA_FLOAT are contiguous
        const uint64_t fmt = godot::Mesh::ARRAY_CUSTOM_R_FLOAT + (components - 1);
        custom_format_flags
            |= fmt
               << (godot::Mesh::ARRAY_FORMAT_CUSTOM_BASE
                   + slot * godot::Mesh::ARRAY_FORMAT_CUSTOM_BITS);
        break;
      }

      default:
        break;
    }
  });

  // Handle index buffer if present
  if constexpr(requires { geom.index; })
  {
    if constexpr(requires { decltype(geom.index)::buffer(); })
    {
      constexpr auto idx_buf_ptr = decltype(geom.index)::buffer();
      auto& idx_buf
          = const_cast<std::decay_t<decltype(geom)>&>(geom).buffers.*idx_buf_ptr;

      // Determine index count
      const int index_count = [&] {
        if constexpr(requires { geom.indices; })
          return geom.indices;
        else if constexpr(requires { idx_buf.element_count; })
          return idx_buf.element_count;
        else if constexpr(requires { idx_buf.size; })
          return idx_buf.size;
        else
          return 0;
      }();

      if(index_count > 0)
      {
        godot::PackedInt32Array indices;
        indices.resize(index_count);
        auto* dst = indices.ptrw();

        if constexpr(requires { idx_buf.elements; })
        {
          for(int i = 0; i < index_count; ++i)
            dst[i] = idx_buf.elements[i];
        }
        else if constexpr(requires { idx_buf.data; })
        {
          for(int i = 0; i < index_count; ++i)
            dst[i] = idx_buf.data[i];
        }

        arrays[godot::Mesh::ARRAY_INDEX] = indices;
      }
    }
    else if constexpr(requires {
                        geom.index.buffer;
                        geom.buffers.size();
                      })
    {
      // Runtime index buffer description (e.g. halp::dynamic_geometry)
      const auto& index = geom.index;

      const int index_count = [&] {
        if constexpr(requires { geom.indices; })
          return geom.indices;
        else
          return 0;
      }();

      if(index.buffer >= 0 && index.buffer < int(geom.buffers.size())
         && index_count > 0)
      {
        const auto& idx_buf = geom.buffers[index.buffer];

        const void* idx_data = nullptr;
        if constexpr(requires { idx_buf.raw_data; })
          idx_data = idx_buf.raw_data;
        else if constexpr(requires { idx_buf.elements; })
          idx_data = idx_buf.elements;

        if(idx_data)
        {
          const char* base
              = static_cast<const char*>(idx_data) + index.byte_offset;

          godot::PackedInt32Array indices;
          indices.resize(index_count);
          auto* dst = indices.ptrw();

          using index_format = std::decay_t<decltype(index.format)>;
          if(index.format == index_format::uint16)
          {
            auto* src = reinterpret_cast<const uint16_t*>(base);
            for(int i = 0; i < index_count; ++i)
              dst[i] = src[i];
          }
          else
          {
            auto* src = reinterpret_cast<const uint32_t*>(base);
            for(int i = 0; i < index_count; ++i)
              dst[i] = src[i];
          }

          arrays[godot::Mesh::ARRAY_INDEX] = indices;
        }
      }
    }
  }

  return arrays;
}

/**
 * Godot Node3D wrapping an Avendish geometry generator/processor.
 *
 * Owns an ArrayMesh and a MeshInstance3D child node.
 * Each frame, runs the processor and rebuilds the mesh from geometry outputs.
 */
template <typename T>
struct godot_geometry_node : public godot::MeshInstance3D
{
  mutable avnd::effect_container<T> effect;

  godot::Ref<godot::ArrayMesh> mesh;

  godot_geometry_node()
  {
    mesh.instantiate();

    if constexpr(avnd::has_inputs<T>)
    {
      for(auto state : effect.full_state())
      {
        avnd::for_each_field_ref(state.inputs, [&]<typename Ctl>(Ctl& ctl) {
          if constexpr(avnd::has_range<Ctl>)
          {
            static_constexpr auto c = avnd::get_range<Ctl>();
            if_possible(ctl.value = c.values[c.init].second)
                else if_possible(ctl.value = c.values[c.init])
                else if_possible(ctl.value = c.init);
          }
        });
      }
    }
  }

  ~godot_geometry_node() override = default;

  void do_ready()
  {
    // Create the MeshInstance3D child
    set_mesh(mesh);

    if constexpr(avnd::has_inputs<T>)
    {
      for(auto state : effect.full_state())
      {
        avnd::for_each_field_ref(state.inputs, [&]<typename Ctl>(Ctl& ctl) {
          if_possible(ctl.update(state.effect));
        });
      }
    }

    rebuild_mesh();
  }

  void do_process(double delta)
  {
    if constexpr(requires { effect.effect({delta}); })
      effect.effect(delta);
    else if constexpr(requires { effect.effect(); })
      effect.effect();

    rebuild_mesh();
  }

  void do_get_property_list(godot::List<godot::PropertyInfo>* p_list) const
  {
    godot_binding::get_property_list<T>(p_list);
  }

  bool do_set(const godot::StringName& p_name, const godot::Variant& p_value)
  {
    return godot_binding::set_property<T>(effect, p_name, p_value);
  }

  bool do_get(const godot::StringName& p_name, godot::Variant& r_ret) const
  {
    return godot_binding::get_property<T>(effect, p_name, r_ret);
  }

  bool do_property_can_revert(const godot::StringName& p_name) const
  {
    return godot_binding::property_can_revert<T>(p_name);
  }

  bool do_property_get_revert(
      const godot::StringName& p_name, godot::Variant& r_property) const
  {
    return godot_binding::property_get_revert<T>(p_name, r_property);
  }

private:
  template <typename Geom>
  void rebuild_mesh_from(Geom& geom)
  {
    // Only rebuild if dirty
    bool dirty = false;
    if constexpr(requires { geom.dirty; })
      dirty = geom.dirty;
    else if constexpr(requires { geom.dirty_mesh; })
      dirty = geom.dirty_mesh;
    else
      dirty = true;

    if(!dirty)
      return;

    uint64_t custom_format_flags = 0;
    auto arrays = build_mesh_arrays(geom, custom_format_flags);

    if(arrays[godot::Mesh::ARRAY_VERTEX].get_type() == godot::Variant::NIL)
      return;

    mesh->clear_surfaces();

    const auto prim = godot_primitive_type(geom);
    mesh->add_surface_from_arrays(
        prim, arrays, {}, {},
        godot::BitField<godot::Mesh::ArrayFormat>(int64_t(custom_format_flags)));

    if constexpr(requires { geom.dirty; })
      geom.dirty = false;
    if constexpr(requires { geom.dirty_mesh; })
      geom.dirty_mesh = false;
  }

  void rebuild_mesh()
  {
    avnd::geometry_output_introspection<T>::for_all(
        [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C>) {
      auto& port = avnd::pfr::get<Idx>(effect.outputs());

      // The geometry data may be the port itself or inside a .mesh member
      if constexpr(avnd::static_geometry_type<C> || avnd::dynamic_geometry_type<C>)
      {
        rebuild_mesh_from(port);
      }
      else if constexpr(requires { port.mesh; })
      {
        rebuild_mesh_from(port.mesh);
      }
    });
  }
};

}

// clang-format off
#define AVND_GODOT_GEOMETRY_NODE_OVERRIDES                                               \
public:                                                                                  \
  void _ready() override { this->do_ready(); }                                           \
  void _process(double delta) override { this->do_process(delta); }                      \
  void _get_property_list(godot::List<godot::PropertyInfo>* p) const                     \
  { this->do_get_property_list(p); }                                                     \
  bool _set(const godot::StringName& n, const godot::Variant& v)                        \
  { return this->do_set(n, v); }                                                         \
  bool _get(const godot::StringName& n, godot::Variant& r) const                        \
  { return this->do_get(n, r); }                                                         \
  bool _property_can_revert(const godot::StringName& n) const                            \
  { return this->do_property_can_revert(n); }                                            \
  bool _property_get_revert(const godot::StringName& n, godot::Variant& r) const        \
  { return this->do_property_get_revert(n, r); }
// clang-format on
