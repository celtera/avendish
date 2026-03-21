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

namespace godot_binding
{

/// Map avendish topology enum tags to Godot primitive types
template <typename Geom>
constexpr godot::Mesh::PrimitiveType godot_primitive_type()
{
  static_constexpr auto m = AVND_ENUM_OR_TAG_MATCHER(
      topology,
      (godot::Mesh::PRIMITIVE_TRIANGLES, triangle, triangles),
      (godot::Mesh::PRIMITIVE_TRIANGLE_STRIP, triangle_strip, triangles_strip),
      (godot::Mesh::PRIMITIVE_LINES, line, lines),
      (godot::Mesh::PRIMITIVE_LINE_STRIP, line_strip),
      (godot::Mesh::PRIMITIVE_POINTS, point, points));
  Geom g{};
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

/// Extract geometry data from an avendish geometry port and build
/// a Godot Array suitable for ArrayMesh::add_surface_from_arrays.
///
/// Strategy: walk the geometry's attributes and input bindings to find
/// position (float3), normal (float3), texcoord (float2), color (float3/4).
/// Read the raw float data from the corresponding buffer + offset.
template <typename Geom>
godot::Array build_mesh_arrays(const Geom& geom)
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

  // Walk attributes and extract data based on location
  auto attributes = avnd::get_attributes(const_cast<std::decay_t<decltype(geom)>&>(geom));
  avnd::for_each_field_ref(
      attributes,
      [&]<typename Attr>(Attr& attr) {
    constexpr int loc = attribute_location<Attr>();

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

    int input_idx = 0;
    avnd::for_each_field_ref(
        const_cast<std::decay_t<decltype(geom)>&>(geom).input,
        [&]<typename Input>(Input& inp) {
      if(input_idx == binding_idx)
      {
        // Get the buffer via the input's buffer() pointer-to-member
        if constexpr(requires { inp.buffer(); })
        {
          constexpr auto buf_ptr = inp.buffer();
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
      auto bindings = avnd::get_bindings(const_cast<std::decay_t<decltype(geom)>&>(geom));
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

    if constexpr(loc == 0) // position — float3
    {
      if(stride_floats == 0)
        stride_floats = 3;
      godot::PackedVector3Array positions;
      positions.resize(vertex_count);
      auto* dst = positions.ptrw();
      for(int i = 0; i < vertex_count; ++i)
      {
        const float* v = data_start + i * stride_floats;
        dst[i] = godot::Vector3(v[0], v[1], v[2]);
      }
      arrays[godot::Mesh::ARRAY_VERTEX] = positions;
    }
    else if constexpr(loc == 3) // normal — float3
    {
      if(stride_floats == 0)
        stride_floats = 3;
      godot::PackedVector3Array normals;
      normals.resize(vertex_count);
      auto* dst = normals.ptrw();
      for(int i = 0; i < vertex_count; ++i)
      {
        const float* v = data_start + i * stride_floats;
        dst[i] = godot::Vector3(v[0], v[1], v[2]);
      }
      arrays[godot::Mesh::ARRAY_NORMAL] = normals;
    }
    else if constexpr(loc == 1) // texcoord — float2
    {
      if(stride_floats == 0)
        stride_floats = 2;
      godot::PackedVector2Array uvs;
      uvs.resize(vertex_count);
      auto* dst = uvs.ptrw();
      for(int i = 0; i < vertex_count; ++i)
      {
        const float* v = data_start + i * stride_floats;
        dst[i] = godot::Vector2(v[0], v[1]);
      }
      arrays[godot::Mesh::ARRAY_TEX_UV] = uvs;
    }
    else if constexpr(loc == 2) // color — float3 or float4
    {
      // Determine component count from datatype if available
      constexpr int components = [] {
        if constexpr(requires { typename Attr::datatype; })
          return sizeof(typename Attr::datatype) / sizeof(float);
        else
          return 3;
      }();

      if(stride_floats == 0)
        stride_floats = components;
      godot::PackedColorArray colors;
      colors.resize(vertex_count);
      auto* dst = colors.ptrw();
      for(int i = 0; i < vertex_count; ++i)
      {
        const float* v = data_start + i * stride_floats;
        if constexpr(components >= 4)
          dst[i] = godot::Color(v[0], v[1], v[2], v[3]);
        else
          dst[i] = godot::Color(v[0], v[1], v[2], 1.0f);
      }
      arrays[godot::Mesh::ARRAY_COLOR] = colors;
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
struct godot_geometry_node : public godot::Node3D
{
  mutable avnd::effect_container<T> effect;

  godot::Ref<godot::ArrayMesh> mesh;
  godot::MeshInstance3D* mesh_instance{nullptr};

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
    mesh_instance = memnew(godot::MeshInstance3D);
    mesh_instance->set_mesh(mesh);
    add_child(mesh_instance);

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

    auto arrays = build_mesh_arrays(geom);

    if(arrays[godot::Mesh::ARRAY_VERTEX].get_type() == godot::Variant::NIL)
      return;

    mesh->clear_surfaces();

    constexpr auto prim = godot_primitive_type<std::decay_t<Geom>>();
    mesh->add_surface_from_arrays(prim, arrays);

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
