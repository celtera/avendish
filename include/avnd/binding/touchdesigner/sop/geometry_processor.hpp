#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>
#include <avnd/binding/touchdesigner/parameter_update.hpp>
#include <avnd/common/export.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/common/enums.hpp>

#include <SOP_CPlusPlusBase.h>

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

    // TODO: Read input geometry if processor has geometry inputs
    // For now, we only support generators (no inputs)

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

  // Info methods
  int32_t getNumInfoCHOPChans(void* reserved) override { return 0; }

  void getInfoCHOPChan(int32_t index, TD::OP_InfoCHOPChan* chan, void* reserved) override
  {
  }

  bool getInfoDATSize(TD::OP_InfoDATSize* infoSize, void* reserved) override
  {
    return false;
  }

  void getInfoDATEntries(
      int32_t index, int32_t nEntries, TD::OP_InfoDATEntries* entries,
      void* reserved) override
  {
  }

  void getWarningString(TD::OP_String* warning, void* reserved) override {}

  void getErrorString(TD::OP_String* error, void* reserved) override {}

  void getInfoPopupString(TD::OP_String* info, void* reserved) override {}

private:
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
        load_geometry(field.mesh, output);
      });
    }
  }

  // Main geometry loading - maps from avnd geometry structure to TD SOP
  template <typename GeomPort>
    requires(avnd::static_geometry_type<GeomPort> || avnd::dynamic_geometry_type<GeomPort>)
  void load_geometry(GeomPort& geom, TD::SOP_Output* output)
  {
    // Get vertex count
    int num_vertices = 0;
    if constexpr(requires { geom.vertices; })
    {
      num_vertices = geom.vertices;
    }

    if(num_vertices == 0)
      return;

    // Build position data
    add_vertices_from_buffers(geom, output);

    // Add primitives based on topology
    add_primitives(geom, output);
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
          buffer_idx = avnd::index_in_struct_static<input.buffer()>();
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
        // We have the buffer - read vertices
        const char* data_ptr = nullptr;

        if constexpr(std::is_same_v<std::decay_t<decltype(buf.data)>, void*>)
        {
          // Dynamic case
          data_ptr = static_cast<const char*>(buf.data);
        }
        else
        {
          // Static case - array of typed data
          using data_type = std::decay_t<decltype(buf.data[0])>;
          data_ptr = reinterpret_cast<const char*>(buf.data);
        }

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
            buffer_idx = avnd::index_in_struct_static<input.buffer()>();
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
          const char* data_ptr = nullptr;

          if constexpr(std::is_same_v<std::decay_t<decltype(buf.data)>, void*>)
          {
            data_ptr = static_cast<const char*>(buf.data);
          }
          else
          {
            using data_type = std::decay_t<decltype(buf.data[0])>;
            data_ptr = reinterpret_cast<const char*>(buf.data);
          }

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
  void add_primitives(GeomPort& geom, TD::SOP_Output* output)
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
            buffer_idx = avnd::index_in_struct_static<input.buffer()>();
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
              // We have the buffer - read vertices
              const char* data_ptr = nullptr;

              for(int k = 0; k < buf.size - 2; k += 3) {
                auto i0 = buf.data[k];
                auto i1 = buf.data[k+1];
                auto i2 = buf.data[k+2];

                output->addTriangle(i0, i1, i2);
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
        output->addTriangle(i+0, i+1, i+2);
      }
    }
    else
    {
      output->addParticleSystem(geom.vertices, 0);
    }
  }

  void update_controls(const TD::OP_Inputs* inputs)
  {
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.update(implementation, inputs);
  }
};

}
