#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>
#include <avnd/binding/touchdesigner/parameter_update.hpp>
#include <avnd/common/export.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>

#include <SOP_CPlusPlusBase.h>

namespace touchdesigner::SOP
{

// Convert to TD::Vector (normals)
template <typename V>
static TD::Vector to_vector(const V& v)
{
  if constexpr(requires { v.x; v.y; v.z; })
  {
    return TD::Vector{
                      static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
  }
  else if constexpr(requires { v[0]; v[1]; v[2]; })
  {
    return TD::Vector{
                      static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2])};
  }
  else if constexpr(avnd::tuple_ish<V> && std::tuple_size_v<V> >= 3)
  {
    return TD::Vector{static_cast<float>(std::get<0>(v)),
                      static_cast<float>(std::get<1>(v)),
                      static_cast<float>(std::get<2>(v))};
  }
  else
  {
    return TD::Vector{0.f, 1.f, 0.f}; // Default up vector
  }
}

// Convert to TD::Color
template <typename C>
static TD::Color to_color(const C& c)
{
  if constexpr(requires { c.r; c.g; c.b; c.a; })
  {
    return TD::Color{static_cast<float>(c.r), static_cast<float>(c.g),
                     static_cast<float>(c.b), static_cast<float>(c.a)};
  }
  else if constexpr(requires { c.r; c.g; c.b; })
  {
    return TD::Color{static_cast<float>(c.r), static_cast<float>(c.g),
                     static_cast<float>(c.b), 1.0f};
  }
  else if constexpr(requires { c[0]; c[1]; c[2]; c[3]; })
  {
    return TD::Color{static_cast<float>(c[0]), static_cast<float>(c[1]),
                     static_cast<float>(c[2]), static_cast<float>(c[3])};
  }
  else if constexpr(requires { c[0]; c[1]; c[2]; })
  {
    return TD::Color{
                     static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2]),
                     1.0f};
  }
  else if constexpr(avnd::tuple_ish<C>)
  {
    if constexpr(std::tuple_size_v<C> >= 4)
    {
      return TD::Color{static_cast<float>(std::get<0>(c)),
                       static_cast<float>(std::get<1>(c)),
                       static_cast<float>(std::get<2>(c)),
                       static_cast<float>(std::get<3>(c))};
    }
    else if constexpr(std::tuple_size_v<C> >= 3)
    {
      return TD::Color{static_cast<float>(std::get<0>(c)),
                       static_cast<float>(std::get<1>(c)),
                       static_cast<float>(std::get<2>(c)), 1.0f};
    }
  }

  // Default: white
  return TD::Color{1.0f, 1.0f, 1.0f, 1.0f};
}

// Convert to TD::TexCoord
template <typename T>
static TD::TexCoord to_texcoord(const T& t)
{
  if constexpr(requires { t.u; t.v; })
  {
    return TD::TexCoord{static_cast<float>(t.u), static_cast<float>(t.v), 0.f};
  }
  else if constexpr(requires { t.u; t.v; t.w; })
  {
    return TD::TexCoord{
                        static_cast<float>(t.u), static_cast<float>(t.v), static_cast<float>(t.w)};
  }
  else if constexpr(requires { t[0]; t[1]; })
  {
    return TD::TexCoord{static_cast<float>(t[0]), static_cast<float>(t[1]), 0.f};
  }
  else if constexpr(avnd::tuple_ish<T> && std::tuple_size_v<T> >= 2)
  {
    return TD::TexCoord{
                        static_cast<float>(std::get<0>(t)), static_cast<float>(std::get<1>(t)), 0.f};
  }
  else
  {
    return TD::TexCoord{0.f, 0.f, 0.f};
  }
}
/**
 * Geometry processor binding for TouchDesigner SOPs
 * Maps Avendish geometry processors to TD's SOP API
 *
 * Uses CPU mode (SOP_Output) for geometry that works in SOP networks
 */
template <typename T>
struct geometry_processor : public TD::SOP_CPlusPlusBase
{
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
      // Iterate over outputs to find geometry ports
      bool found_geometry = false;

      avnd::output_introspection<T>::for_all(
          avnd::get_outputs(implementation),
          [&]<typename Field>(Field& field) {
            if constexpr(avnd::geometry_port<Field>)
            {
              write_geometry_port(output, field);
              found_geometry = true;
            }
            else if constexpr(requires { field.mesh; } && avnd::geometry_port<decltype(field.mesh)>)
            {
              write_geometry_port(output, field.mesh);
              found_geometry = true;
            }
          });
    }
  }

  // Write a single geometry port to output
  template <typename GeomPort>
  void write_geometry_port(TD::SOP_Output* output, GeomPort& geom)
  {
    // Add vertices/points
    if constexpr(requires { geom.vertices; })
    {
      using vertex_type = typename std::decay_t<decltype(geom.vertices)>::value_type;

      // Add all vertices
      for(const auto& vertex : geom.vertices)
      {
        TD::Position pos = to_position(vertex);
        output->addPoint(pos);
      }

      // Add vertex attributes if they exist
      if constexpr(requires { geom.attributes; })
      {
        write_attributes(output, geom.attributes, geom.vertices.size());
      }

      // Add primitives (indices define triangles)
      if constexpr(requires { geom.indices; })
      {
        const auto& indices = geom.indices;
        const int num_triangles = indices.size() / 3;

        if(num_triangles > 0)
        {
          output->addTriangles(indices.data(), num_triangles);
        }
      }
    }
  }

  // Convert various vertex types to TD::Position
  template <typename V>
  TD::Position to_position(const V& v)
  {
    if constexpr(requires { v.x; v.y; v.z; })
    {
      return TD::Position{
          static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
    }
    else if constexpr(requires { v[0]; v[1]; v[2]; })
    {
      return TD::Position{
          static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2])};
    }
    else if constexpr(avnd::tuple_ish<V> && std::tuple_size_v<V> >= 3)
    {
      return TD::Position{static_cast<float>(std::get<0>(v)),
                          static_cast<float>(std::get<1>(v)),
                          static_cast<float>(std::get<2>(v))};
    }
    else
    {
      // Fallback: return origin
      return TD::Position{0.f, 0.f, 0.f};
    }
  }

  // Write vertex attributes (normals, colors, texcoords)
  template <typename Attribs>
  void write_attributes(TD::SOP_Output* output, const Attribs& attribs, int num_vertices)
  {
    // Write normals
    if constexpr(requires { attribs.normals; })
    {
      const auto& normals = attribs.normals;
      if(normals.size() == num_vertices)
      {
        for(int i = 0; i < num_vertices; ++i)
        {
          output->setNormal(to_vector(normals[i]), i);
        }
      }
    }

    // Write colors
    if constexpr(requires { attribs.colors; })
    {
      const auto& colors = attribs.colors;
      if(colors.size() == num_vertices)
      {
        for(int i = 0; i < num_vertices; ++i)
        {
          output->setColor(to_color(colors[i]), i);
        }
      }
    }

    // Write texture coordinates
    if constexpr(requires { attribs.texcoords; })
    {
      const auto& texcoords = attribs.texcoords;
      if(texcoords.size() == num_vertices)
      {
        for(int i = 0; i < num_vertices; ++i)
        {
          TD::TexCoord tc = to_texcoord(texcoords[i]);
          output->setTexCoord(&tc, 1, i);
        }
      }
    }
  }


  void update_controls(const TD::OP_Inputs* inputs)
  {
    if constexpr(avnd::has_inputs<T>)
      parameter_update<T>{}.update(implementation, inputs);
  }
};

} // namespace touchdesigner::SOP
