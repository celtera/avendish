#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/geometry_helpers.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/file_ports.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>
#include <avnd/binding/touchdesigner/parameter_update.hpp>
#include <avnd/binding/touchdesigner/info_output.hpp>
#include <avnd/common/export.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/common/enums.hpp>

#include <SOP_CPlusPlusBase.h>

#include <string>
#include <vector>

namespace touchdesigner::SOP
{
enum class TopologyType {
  triangles, points
};

template <typename T>
constexpr auto get_topology(const T& t) -> TopologyType
{
  static_constexpr auto m = AVND_ENUM_OR_TAG_MATCHER(
      topology, (TopologyType::triangles, triangle, triangles), (TopologyType::points, point, points));
  return m(t, TopologyType::triangles);
}

/**
 * Geometry processor binding for TouchDesigner SOPs
 * Maps Avendish geometry processors (with buffers/bindings/attributes structure)
 * to TD's SOP API
 *
 * Uses CPU mode (SOP_Output) for geometry that works in SOP networks
 */
template <typename T>
struct geometry_processor : public TD::SOP_CPlusPlusBase
{
  static_assert(avnd::geometry_output_introspection<T>::size == 1);
  avnd::effect_container<T> implementation;
  parameter_setup<T> param_setup;
  touchdesigner::file_ports<T> file_setup;

  // Owns the triangle index buffers built from the input SOP primitives so the
  // avnd geometry-input ports can point at them for the duration of the cook.
  std::vector<std::vector<uint32_t>> sop_index_storage;

  explicit geometry_processor(const TD::OP_NodeInfo* info)
  {
    init();
  }

  void init()
  {
    avnd::init_controls(implementation);
  }

  void getGeneralInfo(
      TD::SOP_GeneralInfo* info, const TD::OP_Inputs* inputs, void* reserved) override
  {
    // Cook every frame if no outputs (e.g., network senders)
    info->cookEveryFrame = false;
    info->cookEveryFrameIfAsked = true;

    // Use CPU mode (not GPU direct)
    info->directToGPU = false;

    // Use counter-clockwise winding (TouchDesigner standard)
    info->winding = TD::SOP_Winding::CCW;
  }

  void execute(TD::SOP_Output* output, const TD::OP_Inputs* inputs, void* reserved) override
  {
    // Update parameter values from TouchDesigner
    update_controls(inputs);

    // Read input geometry into the object's geometry input ports
    read_input_geometry(inputs);

    // Execute the processor
    struct tick
    {
      int frames = 0;
    } t{.frames = 1};

    invoke_effect(implementation, t);

    // Write output geometry
    if constexpr(avnd::has_outputs<T>)
    {
      write_geometry(output);
    }
  }

  void executeVBO(
      TD::SOP_VBOOutput* output, const TD::OP_Inputs* inputs, void* reserved) override
  {
    // GPU direct mode - not implemented yet
    // For now, this should never be called since directToGPU = false
  }

  void setupParameters(TD::OP_ParameterManager* manager, void* reserved) override
  {
    param_setup.setup(implementation, manager);
    file_setup.setup(implementation, manager);
  }

  void pulsePressed(const char* name, void* reserved) override
  {
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.pulse(implementation, name);
  }

  void buildDynamicMenu(
      const TD::OP_Inputs* inputs, TD::OP_BuildDynamicMenuInfo* info,
      void* reserved) override
  {
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.menu(implementation, inputs, info);
  }

  // Info methods — route non-primary outputs to Info CHOP/DAT
  int32_t getNumInfoCHOPChans(void* reserved) override
  {
    return info_output<T>::get_num_info_chop_chans(implementation);
  }

  void getInfoCHOPChan(int32_t index, TD::OP_InfoCHOPChan* chan, void* reserved) override
  {
    info_output<T>::get_info_chop_chan(implementation, index, chan);
  }

  bool getInfoDATSize(TD::OP_InfoDATSize* infoSize, void* reserved) override
  {
    return info_output<T>::get_info_dat_size(implementation, infoSize);
  }

  void getInfoDATEntries(
      int32_t index, int32_t nEntries, TD::OP_InfoDATEntries* entries,
      void* reserved) override
  {
    info_output<T>::get_info_dat_entries(implementation, index, nEntries, entries);
  }

  void getWarningString(TD::OP_String* warning, void* reserved) override {}

  void getErrorString(TD::OP_String* error, void* reserved) override {}

  void getInfoPopupString(TD::OP_String* info, void* reserved) override {}

private:
  // A geometry port is either the geometry itself or a struct wrapping it in a
  // `.mesh` member.
  template <typename Field>
  static auto& resolve_mesh(Field& field)
  {
    if constexpr(requires { field.mesh; })
      return field.mesh;
    else
      return field;
  }

  // Read TD SOP input geometry into the object's geometry input ports. Only CPU
  // dynamic_geometry is filled (point position/normal/color); GPU geometry is
  // handled by the POP/VBO path. The TD-owned arrays stay valid for this cook.
  void read_input_geometry(const TD::OP_Inputs* inputs)
  {
    if constexpr(avnd::geometry_input_introspection<T>::size > 0)
    {
      sop_index_storage.resize(avnd::geometry_input_introspection<T>::size);
      int input_index = 0;
      avnd::geometry_input_introspection<T>::for_all(
          avnd::get_inputs(implementation), [&]<typename Field>(Field& field) {
        const int slot = input_index++;
        if(const TD::OP_SOPInput* sop = inputs->getInputSOP(slot))
          load_sop_input(sop, field, slot);
      });
    }
  }

  template <typename Field>
  void load_sop_input(const TD::OP_SOPInput* sop, Field& field, int slot)
  {
    auto& geom = resolve_mesh(field);
    using geom_t = std::decay_t<decltype(geom)>;
    if constexpr(avnd::dynamic_geometry_type<geom_t>)
    {
      const int32_t num_points = sop->getNumPoints();
      const TD::Position* pos = sop->getPointPositions();
      if(num_points <= 0 || !pos)
        return;

      if constexpr(requires { geom.vertices; })
        geom.vertices = num_points;

      // Only host (raw_data) buffers can take CPU point data.
      if constexpr(requires { geom.buffers.resize(1); geom.buffers[0].raw_data; })
      {
        struct src { const void* data; int comps; attribute_semantic sem; attribute_format fmt; };
        src list[3];
        int n = 0;
        list[n++] = {pos, 3, attribute_semantic::position, attribute_format::float3};

        if(const TD::SOP_NormalInfo* nrm = sop->getNormals();
           nrm && nrm->normals && nrm->numNormals >= num_points)
          list[n++] = {nrm->normals, 3, attribute_semantic::normal, attribute_format::float3};
        if(const TD::SOP_ColorInfo* col = sop->getColors();
           col && col->colors && col->numColors >= num_points)
          list[n++] = {col->colors, 4, attribute_semantic::color0, attribute_format::float4};

        geom.buffers.resize(n);
        geom.bindings.resize(n);
        geom.input.resize(n);
        geom.attributes.resize(n);
        for(int i = 0; i < n; i++)
        {
          auto& buf = geom.buffers[i];
          buf.raw_data = const_cast<void*>(list[i].data);
          buf.byte_size = (int64_t)num_points * list[i].comps * sizeof(float);
          if constexpr(requires { buf.dirty; })
            buf.dirty = true;

          geom.bindings[i].stride = list[i].comps * sizeof(float);
          geom.bindings[i].step_rate = 1;

          geom.input[i].buffer = i;
          geom.input[i].byte_offset = 0;

          auto& a = geom.attributes[i];
          a.binding = i;
          a.byte_offset = 0;
          a.semantic = decltype(a.semantic)(list[i].sem);
          a.format = decltype(a.format)(list[i].fmt);
          if constexpr(requires { a.name; })
            a.name.clear();
        }

        // Read the input topology into an index buffer so a filter sees faces,
        // not just a point cloud. n-gons are fan-triangulated. Additive: it only
        // populates geom.index (previously unset); ports that ignore indices are
        // unaffected.
        if constexpr(requires { geom.index.buffer; })
        {
          const int32_t num_prims = sop->getNumPrimitives();
          auto& idx = sop_index_storage[slot];
          idx.clear();
          for(int32_t p = 0; p < num_prims; ++p)
          {
            const TD::SOP_PrimitiveInfo& pi = sop->getPrimitive(p);
            if(!pi.pointIndices || pi.numVertices < 3)
              continue;
            for(int32_t v = 1; v + 1 < pi.numVertices; ++v)
            {
              idx.push_back(static_cast<uint32_t>(pi.pointIndices[0]));
              idx.push_back(static_cast<uint32_t>(pi.pointIndices[v]));
              idx.push_back(static_cast<uint32_t>(pi.pointIndices[v + 1]));
            }
          }
          if(!idx.empty())
          {
            const int bidx = static_cast<int>(geom.buffers.size());
            geom.buffers.push_back({});
            auto& ib = geom.buffers[bidx];
            ib.raw_data = idx.data();
            ib.byte_size = static_cast<int64_t>(idx.size()) * sizeof(uint32_t);
            if constexpr(requires { ib.dirty; })
              ib.dirty = true;
            geom.index.buffer = bidx;
            geom.index.byte_offset = 0;
            geom.index.format = decltype(geom.index.format)(1); // uint32
            if constexpr(requires { geom.indices; })
              geom.indices = static_cast<int>(idx.size());
          }
        }
      }
    }
  }

  // Write Avendish geometry to SOP output
  void write_geometry(TD::SOP_Output* output)
  {
    // Find geometry outputs
    if constexpr(avnd::has_outputs<T>)
    {
      // There's only one but well
      avnd::geometry_output_introspection<T>::for_all(
          avnd::get_outputs(implementation),
          [&]<typename Field>(Field& field) {
        // A geometry port is either the geometry itself or a struct wrapping it
        // in a `.mesh` member (optionally a vector of meshes).
        if constexpr(requires { field.mesh; })
        {
          if constexpr(avnd::static_geometry_type<decltype(field.mesh)>
                       || avnd::dynamic_geometry_type<decltype(field.mesh)>)
            load_geometry(field.mesh, output);
          else
          {
            // Vector of meshes: emit them all, offsetting each mesh's primitive
            // indices by the running point count (SOP_Output point indices
            // accumulate across addPoint calls).
            int base_point = 0;
            for(auto& m : field.mesh)
            {
              load_geometry(m, output, base_point);
              if constexpr(requires { m.vertices; })
                base_point += m.vertices;
            }
          }
        }
        else
        {
          load_geometry(field, output);
        }
      });
    }
  }

  // Main geometry loading - maps from avnd geometry structure to TD SOP
  template <typename GeomPort>
    requires(avnd::static_geometry_type<GeomPort> || avnd::dynamic_geometry_type<GeomPort>)
  // base_point is the running point index of the first point this mesh adds to
  // the output (0 for a single mesh; for a vector of meshes it accumulates so
  // each mesh's primitive indices stay correct, since SOP_Output point indices
  // accumulate across addPoint calls).
  void load_geometry(GeomPort& geom, TD::SOP_Output* output, int base_point = 0)
  {
    // Get vertex count
    int num_vertices = 0;
    if constexpr(requires { geom.vertices; })
    {
      num_vertices = geom.vertices;
    }

    if(num_vertices == 0)
      return;

    if constexpr(avnd::dynamic_geometry_type<GeomPort>)
    {
      // Run-time attribute list (e.g. halp::dynamic_geometry):
      // attributes are identified by their semantic
      add_vertices_from_dynamic_buffers(geom, output);
      add_dynamic_primitives(geom, output, base_point);
    }
    else
    {
      // Build position data
      add_vertices_from_buffers(geom, output);

      // Add primitives based on topology
      add_primitives(geom, output, base_point);
    }
  }

  // Dynamic geometry: locate the position attribute through its semantic
  // and add the points, then the remaining attributes
  template <typename GeomPort>
  void add_vertices_from_dynamic_buffers(GeomPort& geom, TD::SOP_Output* output)
  {
    const int num_vertices = geom.vertices;

    for(const auto& attr : geom.attributes)
    {
      if(get_attribute_semantic(attr) != attribute_semantic::position)
        continue;

      const char* data{};
      int stride{};
      if(!resolve_dynamic_attribute(geom, attr, data, stride))
        return;

      if(stride == 3 * sizeof(float))
      {
        const auto* vertex_data = reinterpret_cast<const TD::Position*>(data);
        output->addPoints(vertex_data, num_vertices);
      }
      else
      {
        for(int i = 0; i < num_vertices; ++i)
        {
          const float* vertex_data = reinterpret_cast<const float*>(data + i * stride);
          output->addPoint(TD::Position{vertex_data[0], vertex_data[1], vertex_data[2]});
        }
      }

      // Now add other attributes (normals, colors, texcoords, customs)
      add_dynamic_attributes(geom, output, num_vertices);
      return;
    }
  }

  template <typename GeomPort>
  void add_dynamic_attributes(GeomPort& geom, TD::SOP_Output* output, int num_vertices)
  {
    for(const auto& attr : geom.attributes)
    {
      const auto sem = get_attribute_semantic(attr);
      // Skip position (already processed)
      if(sem == attribute_semantic::position)
        continue;

      const char* data{};
      int stride{};
      if(!resolve_dynamic_attribute(geom, attr, data, stride))
        continue;

      const auto fmt = get_attribute_format(attr);
      const int components = format_components(fmt);
      const auto kind = format_data_kind(fmt);

      switch(sem)
      {
        case attribute_semantic::normal: {
          if(kind != attribute_data_kind::f32 || components < 3)
            break;
          for(int i = 0; i < num_vertices; ++i)
          {
            const float* normal_data = reinterpret_cast<const float*>(data + i * stride);
            output->setNormal(TD::Vector{normal_data[0], normal_data[1], normal_data[2]}, i);
          }
          break;
        }
        case attribute_semantic::color0: {
          if(kind != attribute_data_kind::f32 || components < 3)
            break;
          for(int i = 0; i < num_vertices; ++i)
          {
            const float* color_data = reinterpret_cast<const float*>(data + i * stride);
            const float alpha = components >= 4 ? color_data[3] : 1.0f;
            output->setColor(TD::Color{color_data[0], color_data[1], color_data[2], alpha}, i);
          }
          break;
        }
        case attribute_semantic::texcoord0: {
          if(kind != attribute_data_kind::f32 || components < 2)
            break;
          for(int i = 0; i < num_vertices; ++i)
          {
            const float* uv_data = reinterpret_cast<const float*>(data + i * stride);
            TD::TexCoord tc{uv_data[0], uv_data[1], components >= 3 ? uv_data[2] : 0.0f};
            output->setTexCoord(&tc, 1, i);
          }
          break;
        }
        default: {
          // Everything else goes through SOP custom attributes
          add_dynamic_custom_attribute(
              output, attr, sem, data, stride, components, kind, num_vertices);
          break;
        }
      }
    }
  }

  // SOP custom attributes only support float and int32 data; bytes, halves
  // and shorts would need a conversion pass and are skipped.
  template <typename Attr>
  void add_dynamic_custom_attribute(
      TD::SOP_Output* output, const Attr& attr, attribute_semantic sem,
      const char* data, int stride, int components, attribute_data_kind kind,
      int num_vertices)
  {
    if(kind == attribute_data_kind::unsupported || components <= 0)
      return;

    std::string name;
    if constexpr(requires { attr.name; })
      if(!attr.name.empty())
        name = attr.name;
    if(name.empty())
      name = semantic_name(sem);

    TD::SOP_CustomAttribData cu;
    cu.name = name.c_str();
    cu.numComponents = components;

    // setCustomAttribute expects tightly packed data
    std::vector<float> floats;
    std::vector<int32_t> ints;
    if(kind == attribute_data_kind::f32)
    {
      floats.resize(std::size_t(num_vertices) * components);
      for(int i = 0; i < num_vertices; ++i)
      {
        const float* src = reinterpret_cast<const float*>(data + i * stride);
        for(int c = 0; c < components; ++c)
          floats[std::size_t(i) * components + c] = src[c];
      }
      cu.attribType = TD::AttribType::Float;
      cu.floatData = floats.data();
    }
    else
    {
      ints.resize(std::size_t(num_vertices) * components);
      for(int i = 0; i < num_vertices; ++i)
      {
        const int32_t* src = reinterpret_cast<const int32_t*>(data + i * stride);
        for(int c = 0; c < components; ++c)
          ints[std::size_t(i) * components + c] = src[c];
      }
      cu.attribType = TD::AttribType::Int;
      cu.intData = ints.data();
    }

    output->setCustomAttribute(&cu, num_vertices);
  }

  template <typename GeomPort>
  void add_dynamic_primitives(GeomPort& geom, TD::SOP_Output* output, int base_point = 0)
  {
    const TopologyType topology = get_topology(geom);
    if(topology == TopologyType::triangles)
    {
      if constexpr(requires { geom.index.buffer; })
      {
        const int buffer_idx = geom.index.buffer;
        if(buffer_idx >= 0 && buffer_idx < int(geom.buffers.size()))
        {
          auto& buf = geom.buffers[buffer_idx];
          if(const char* data = geometry_buffer_data(buf))
          {
            data += geom.index.byte_offset;

            // index_format: uint16 == 0, uint32 == 1
            const bool idx16 = static_cast<uint32_t>(geom.index.format) == 0;
            int64_t num_indices = 0;
            if constexpr(requires { geom.indices; })
              num_indices = geom.indices;
            if(num_indices <= 0)
              num_indices = (geometry_buffer_byte_size(buf) - geom.index.byte_offset)
                            / (idx16 ? 2 : 4);

            if(num_indices >= 3)
            {
              if(idx16)
              {
                const auto* indices = reinterpret_cast<const uint16_t*>(data);
                for(int64_t k = 0; k < num_indices - 2; k += 3)
                  output->addTriangle(
                      indices[k] + base_point, indices[k + 1] + base_point,
                      indices[k + 2] + base_point);
              }
              else
              {
                const auto* indices = reinterpret_cast<const uint32_t*>(data);
                for(int64_t k = 0; k < num_indices - 2; k += 3)
                  output->addTriangle(
                      indices[k] + base_point, indices[k + 1] + base_point,
                      indices[k + 2] + base_point);
              }
              // Index buffer used successfully
              return;
            }
          }
        }
      }

      // No usable index buffer: add the triangles directly
      for(int i = 0; i < geom.vertices - 2; i += 3)
      {
        output->addTriangle(base_point + i + 0, base_point + i + 1, base_point + i + 2);
      }
    }
    else
    {
      output->addParticleSystem(geom.vertices, base_point);
    }
  }

  template <typename GeomPort>
  void add_vertices_from_buffers(GeomPort& geom, TD::SOP_Output* output)
  {
    int& num_vertices = geom.vertices;
    // Find the position attribute
    int position_binding_idx = -1;
    int position_offset = 0;
    bool found_position = false;

    avnd::for_each_field_ref(avnd::get_attributes(geom), [&]<typename Attr>(Attr& attr) {
      // Check if this is a position attribute
      if constexpr(requires { Attr::position; } || requires { Attr::positions; })
      {
        if constexpr(requires { attr.binding(); })
          position_binding_idx = attr.binding();
        else if constexpr(requires { attr.binding; })
          position_binding_idx = attr.binding;
        else
          position_binding_idx = 0;

        if constexpr(requires { attr.byte_offset(); })
          position_offset = attr.byte_offset();
        else if constexpr(requires { attr.byte_offset; })
          position_offset = attr.byte_offset;
        else
          position_offset = 0;

        found_position = true;
      }
    });

    if(!found_position)
      return;

    // Get the binding for positions to know the stride
    int position_stride = 0;
    int binding_idx = 0;
    avnd::for_each_field_ref(avnd::get_bindings(geom), [&](auto& binding) {
      if(binding_idx == position_binding_idx)
      {
        position_stride = binding.stride;
      }
      binding_idx++;
    });

    // Get the input that references the buffer for positions
    int buffer_idx = 0;
    int buffer_offset = 0;
    int input_idx = 0;
    avnd::for_each_field_ref(geom.input, [&](auto& input) {
      if(input_idx == position_binding_idx)
      {
        if constexpr(requires { input.buffer(); })
          buffer_idx = avnd::index_in_struct_static<std::decay_t<decltype(input)>::buffer()>();
        else if constexpr(requires { input.buffer; })
          buffer_idx = input.buffer;

        if constexpr(requires { input.byte_offset(); })
          buffer_offset = input.byte_offset();
        else if constexpr(requires { input.byte_offset; })
          buffer_offset = input.byte_offset;
      }
      input_idx++;
    });

    // Now read the actual buffer data and add vertices
    int buf_idx = 0;
    avnd::for_each_field_ref(geom.buffers, [&](auto& buf) {
      if(buf_idx == buffer_idx)
      {
        // We have the buffer - read vertices.
        // Buffers are either raw (raw_data / byte_size) or typed
        // (elements / element_count).
        const char* data_ptr = geometry_buffer_data(buf);

        if(!data_ptr) {
          buf_idx++;
          return;
        }

        // Add vertices
        if(position_stride == 3 * sizeof(float))
        {
          const TD::Position* vertex_data = reinterpret_cast<const TD::Position*>(
              data_ptr + buffer_offset + position_offset);
          output->addPoints(vertex_data, num_vertices);
        }
        else
        {
          for(int i = 0; i < num_vertices; ++i)
          {
            const float* vertex_data = reinterpret_cast<const float*>(
                data_ptr + buffer_offset + position_offset + i * position_stride);

            TD::Position pos{vertex_data[0], vertex_data[1], vertex_data[2]};
            output->addPoint(pos);
          }
        }

        // Now add other attributes (normals, colors, texcoords)
        add_attributes_from_buffers(geom, output, num_vertices);
      }
      buf_idx++;
    });
  }

  template <typename GeomPort>
  void add_attributes_from_buffers(GeomPort& geom, TD::SOP_Output* output, int num_vertices)
  {
    // Process each attribute
    avnd::for_each_field_ref(avnd::get_attributes(geom), [&]<typename Attr>(Attr& attr) {
      // Skip position (already processed)
      if constexpr(requires { Attr::position; } || requires { Attr::positions; })
      {
        return;
      }

      // Get attribute metadata
      int attr_binding = 0;
      int attr_offset = 0;

      if constexpr(requires { attr.binding; })
        attr_binding = attr.binding;
      if constexpr(requires { attr.byte_offset; })
        attr_offset = attr.byte_offset;

      // Get stride from binding
      int attr_stride = 0;
      int binding_idx = 0;
      avnd::for_each_field_ref(avnd::get_bindings(geom), [&](auto& binding) {
        if(binding_idx == attr_binding)
        {
          attr_stride = binding.stride;
        }
        binding_idx++;
      });

      // Get buffer info from input
      int buffer_idx = 0;
      int buffer_offset = 0;
      int input_idx = 0;
      avnd::for_each_field_ref(geom.input, [&](auto& input) {
        if(input_idx == attr_binding)
        {
          if constexpr(requires { input.buffer(); })
            buffer_idx = avnd::index_in_struct_static<std::decay_t<decltype(input)>::buffer()>();
          else if constexpr(requires { input.buffer; })
            buffer_idx = input.buffer;

          if constexpr(requires { input.byte_offset(); })
            buffer_offset = input.byte_offset();
          else if constexpr(requires { input.byte_offset; })
            buffer_offset = input.byte_offset;
        }
        input_idx++;
      });

      // Read buffer and set attribute
      int buf_idx = 0;
      avnd::for_each_field_ref(geom.buffers, [&](auto& buf) {
        if(buf_idx == buffer_idx)
        {
          const char* data_ptr = geometry_buffer_data(buf);

          if(!data_ptr) {
            buf_idx++;
            return;
          }

          // Set attribute based on type
          if constexpr(requires { Attr::normal; } || requires { Attr::normals; })
          {
            // Normals
            for(int i = 0; i < num_vertices; ++i)
            {
              const float* normal_data = reinterpret_cast<const float*>(
                  data_ptr + buffer_offset + attr_offset + i * attr_stride);
              TD::Vector normal{normal_data[0], normal_data[1], normal_data[2]};
              output->setNormal(normal, i);
            }
          }
          else if constexpr(requires { Attr::color; } || requires { Attr::colors; })
          {
            // Colors
            for(int i = 0; i < num_vertices; ++i)
            {
              const float* color_data = reinterpret_cast<const float*>(
                  data_ptr + buffer_offset + attr_offset + i * attr_stride);

              // Determine number of components
              using datatype = typename Attr::datatype;
              TD::Color color;
              if constexpr(std::is_same_v<datatype, float[4]>)
              {
                color = TD::Color{color_data[0], color_data[1], color_data[2], color_data[3]};
              }
              else if constexpr(std::is_same_v<datatype, float[3]>)
              {
                color = TD::Color{color_data[0], color_data[1], color_data[2], 1.0f};
              }
              else
              {
                color = TD::Color{1.0f, 1.0f, 1.0f, 1.0f};
              }
              output->setColor(color, i);
            }
          }
          else if constexpr(
              requires { Attr::texcoord; } || requires { Attr::tex_coord; }
              || requires { Attr::texcoords; } || requires { Attr::tex_coords; }
              || requires { Attr::uv; } || requires { Attr::uvs; })
          {
            // Texture coordinates
            for(int i = 0; i < num_vertices; ++i)
            {
              const float* uv_data = reinterpret_cast<const float*>(
                  data_ptr + buffer_offset + attr_offset + i * attr_stride);
              TD::TexCoord tc{uv_data[0], uv_data[1], 0.0f};
              output->setTexCoord(&tc, 1, i);
            }
          }
        }
        buf_idx++;
      });
    });
  }

  template <typename GeomPort>
  void add_primitives(GeomPort& geom, TD::SOP_Output* output, int base_point = 0)
  {
    const TopologyType topology = get_topology(geom);
    if(topology == TopologyType::triangles)
    {
      if constexpr(requires { geom.index; })
      {
        // Get the input that references the buffer for indices
        int buffer_idx = -1;
        int buffer_offset = 0;
        auto& input = geom.index;
        {
          if constexpr(requires { input.buffer(); })
            buffer_idx = avnd::index_in_struct_static<std::decay_t<decltype(input)>::buffer()>();
          else if constexpr(requires { input.buffer; })
            buffer_idx = input.buffer;

          if constexpr(requires { input.byte_offset(); })
            buffer_offset = input.byte_offset();
          else if constexpr(requires { input.byte_offset; })
            buffer_offset = input.byte_offset;
        }

        // Read the indices 3 by 3
        if(buffer_idx >= 0)
        {
          // Now read the actual buffer data and add vertices
          int buf_idx = 0;
          avnd::for_each_field_ref(geom.buffers, [&](auto& buf) {
            if(buf_idx == buffer_idx)
            {
              // We have the buffer - read the indices
              if constexpr(requires { buf.elements[0]; buf.element_count; })
              {
                for(int k = 0; k < buf.element_count - 2; k += 3)
                {
                  auto i0 = buf.elements[k];
                  auto i1 = buf.elements[k + 1];
                  auto i2 = buf.elements[k + 2];

                  output->addTriangle(i0 + base_point, i1 + base_point, i2 + base_point);
                }
              }
              else if constexpr(requires { buf.raw_data; buf.byte_size; })
              {
                // Raw buffer: interpret as tightly packed uint32 indices
                if(const auto* indices = static_cast<const uint32_t*>(buf.raw_data))
                {
                  const int64_t count = buf.byte_size / int64_t(sizeof(uint32_t));
                  for(int64_t k = 0; k < count - 2; k += 3)
                    output->addTriangle(
                        indices[k] + base_point, indices[k + 1] + base_point,
                        indices[k + 2] + base_point);
                }
              }
            }
            buf_idx++;
          });
          // Index buffer used successfully
          return;
        }
      }

      // If we're here it means we could not add the triangles through an index,
      // so instead let's add them manually
      for(int i = 0; i < geom.vertices - 2; i += 3)
      {
        output->addTriangle(base_point + i+0, base_point + i+1, base_point + i+2);
      }
    }
    else
    {
      output->addParticleSystem(geom.vertices, base_point);
    }
  }

  void update_controls(const TD::OP_Inputs* inputs)
  {
    if constexpr(avnd::has_inputs<T>)
    {
      parameter_update<T>{}.update(implementation, inputs);
      file_setup.load(implementation, inputs);
    }
  }
};

}
