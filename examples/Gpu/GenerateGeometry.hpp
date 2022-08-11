#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <avnd/concepts/gfx.hpp>
#include <random>
#include <cstdlib>
#include <cstring>

// Some performance pointers:

// NVidia recommendations: interleave
// https://docs.nvidia.com/tegra/Content/GLES2_Perf_Vertex_Shader.html

// Apple recommendations: interleave and separate dynamically changing data
// https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/OpenGLES_ProgrammingGuide/TechniquesforWorkingwithVertexData/TechniquesforWorkingwithVertexData.html

// AMD recommendations:
// https://www.reddit.com/r/vulkan/comments/rtpdvu/interleaved_vs_separate_vertex_buffers/
// - 1 stream for position only (& everything required for computing gl_Position)
// - the rest as another interleaved stream

// ARM: same than AMD
// https://developer.arm.com/documentation/102537/0100/Attribute-Buffer-Encoding
// https://developer.qualcomm.com/sites/default/files/docs/adreno-gpu/developer-guide/gpu/best_practices_vertex.html

// Other:
// https://anteru.net/blog/2016/storing-vertex-data-to-interleave-or-not-to-interleave/
// https://stackoverflow.com/questions/14535413/how-does-interleaved-vertex-submission-help-performance

namespace examples
{
// Some cube data
static constexpr float default_cube_position[] = {
    -1.0f,-1.0f,-1.0f,
    -1.0f,-1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    1.0f, 1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f,-1.0f,
    1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f,-1.0f,
    1.0f,-1.0f,-1.0f,
    1.0f, 1.0f,-1.0f,
    1.0f,-1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,-1.0f,
    1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,
    1.0f,-1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f,-1.0f,-1.0f,
    1.0f, 1.0f,-1.0f,
    1.0f,-1.0f,-1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f,-1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f,-1.0f,
    -1.0f, 1.0f,-1.0f,
    1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,-1.0f,
    -1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    1.0f,-1.0f, 1.0f
};

static constexpr float default_cube_texcoord[] = {
    0.000059f, 1.0f - 0.000004f,
    0.000103f, 1.0f - 0.336048f,
    0.335973f, 1.0f - 0.335903f,
    1.000023f, 1.0f - 0.000013f,
    0.667979f, 1.0f - 0.335851f,
    0.999958f, 1.0f - 0.336064f,
    0.667979f, 1.0f - 0.335851f,
    0.336024f, 1.0f - 0.671877f,
    0.667969f, 1.0f - 0.671889f,
    1.000023f, 1.0f - 0.000013f,
    0.668104f, 1.0f - 0.000013f,
    0.667979f, 1.0f - 0.335851f,
    0.000059f, 1.0f - 0.000004f,
    0.335973f, 1.0f - 0.335903f,
    0.336098f, 1.0f - 0.000071f,
    0.667979f, 1.0f - 0.335851f,
    0.335973f, 1.0f - 0.335903f,
    0.336024f, 1.0f - 0.671877f,
    1.000004f, 1.0f - 0.671847f,
    0.999958f, 1.0f - 0.336064f,
    0.667979f, 1.0f - 0.335851f,
    0.668104f, 1.0f - 0.000013f,
    0.335973f, 1.0f - 0.335903f,
    0.667979f, 1.0f - 0.335851f,
    0.335973f, 1.0f - 0.335903f,
    0.668104f, 1.0f - 0.000013f,
    0.336098f, 1.0f - 0.000071f,
    0.000103f, 1.0f - 0.336048f,
    0.000004f, 1.0f - 0.671870f,
    0.336024f, 1.0f - 0.671877f,
    0.000103f, 1.0f - 0.336048f,
    0.336024f, 1.0f - 0.671877f,
    0.335973f, 1.0f - 0.335903f,
    0.667969f, 1.0f - 0.671889f,
    1.000004f, 1.0f - 0.671847f,
    0.667979f, 1.0f - 0.335851f
};

// In this example the vertex buffer has
// all the position attributes, then all the texcoord attributes
struct non_indexed_split_geometry
{
  static consteval auto name() { return "Geometry"; }

  struct buffers {
    struct {
      enum { dynamic, vertex };
      float* data{};
      int size{};
      bool dirty{};
    } main_buffer;
  } buffers;

  // This example uses two successive bindings to one buffer.
  struct bindings {
    struct {
      enum { per_vertex };
      int stride = 3 * sizeof(float);
      int step_rate = 1;
    } position_binding;

    struct {
      enum { per_vertex };
      int stride = 2 * sizeof(float);
      int step_rate = 1;
    } texcoord_binding;
  };

  struct attributes {
    struct
    {
      enum { position };
      using datatype = float[3];

      // If we have non-interleaved buffers, e.g. each individual attribute is contiguous
      // offset is 0. Otherwise it's at which point this attribute starts in the buffer data.
      // e.g. if the buffer is [ [ x y z ] [ u v ] [ x y z ] [ u v ] ... ]
      // then the offset for position is 0 and the offset for texcoord is 3 * sizeof(float)
      int32_t offset = 0;
      int32_t binding = 0;
    } position;

    struct {
      enum { tex_coord };
      using datatype = float[2];
      int32_t binding = 1;
    } texcoord;
  };

  struct {
    struct {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int offset = 0;
    } input0;
    struct {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int offset = sizeof(default_cube_position);
    } input1;
  } input;
  int vertices = 0;
  bool dirty{};
  enum { triangles, counter_clockwise, cull_back };
};

struct GenerateGeometryNonInterleaved
{
  static consteval auto name() { return "Generate Geometry"; }
  static consteval auto c_name() { return "avnd_generate_geometry"; }
  static consteval auto uuid() { return "253aa25d-db82-4295-9156-386322573c65"; }

  struct
  {
    struct
    {
      static consteval auto name() { return "Radius"; }
      struct range { double min = 0.001, max = 2, init = 0.1; };
      void update(GenerateGeometryNonInterleaved& self) {

        for(int i = 0; i < std::ssize(default_cube_position); i++)
        {
          self.outputs.geometry.buffers.main_buffer.data[i] = self.inputs.radius.value * default_cube_position[i];
        }

        self.outputs.geometry.buffers.main_buffer.dirty = true;
        self.outputs.geometry.dirty = true;
      }

      float value;
    } radius;
  } inputs;


  struct
  {
    non_indexed_split_geometry geometry;
    static_assert(avnd::geometry_port<decltype(geometry)>);
  } outputs;


  GenerateGeometryNonInterleaved()
  {
    constexpr auto float_count = std::ssize(default_cube_position) + std::ssize(default_cube_texcoord);
    outputs.geometry.buffers.main_buffer = {
        .data = new float[float_count],
        .size = float_count,
        .dirty = true
    };
    memcpy(outputs.geometry.buffers.main_buffer.data, default_cube_position, sizeof(default_cube_position));
    memcpy(outputs.geometry.buffers.main_buffer.data + std::ssize(default_cube_position), default_cube_texcoord, sizeof(default_cube_texcoord));

    outputs.geometry.input.input0.offset = 0;
    outputs.geometry.input.input1.offset = sizeof(default_cube_position);

    outputs.geometry.vertices = std::ssize(default_cube_position) / 3;
  }

  ~GenerateGeometryNonInterleaved()
  {
    delete outputs.geometry.buffers.main_buffer.data;
  }

  void operator()() {

  }
};


///////////////
///////////////
///////////////




// Some cube data
static constexpr float indexed_cube_position[] = {
    -1.0f,1.0f,1.0f,
    -1.0f,-1.0f,1.0f,
    1.0f,1.0f,1.0f,
    1.0f,-1.0f,1.0f,
    -1.0f,1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,
    1.0f,1.0f,-1.0f,
    1.0f,-1.0f,-1.0f
};
static constexpr uint16_t indexed_cube_index[] = {
    0, 2, 3, 0, 3, 1,
    2, 6, 7, 2, 7, 3,
    6, 4, 5, 6, 5, 7,
    4, 0, 1, 4, 1, 5,
    0, 4, 6, 0, 6, 2,
    1, 5, 7, 1, 7, 3,
};

static constexpr float indexed_cube_texcoord[] = {
   0.335973f, 1.0f - 0.335903f,
   0.000103f, 1.0f - 0.336048f,
   0.668104f, 1.0f - 0.000013f,
   0.667979f, 1.0f - 0.335851f,
   0.336098f, 1.0f - 0.000071f,
   0.000059f, 1.0f - 0.000004f,
   1.000023f, 1.0f - 0.000013f,
   0.667969f, 1.0f - 0.671889f,
};

// In this example the vertex buffer has
// all the position attributes, then all the texcoord attributes
struct indexed_split_geometry
{
  static consteval auto name() { return "Geometry"; }

  struct buffers {
    struct {
      enum { dynamic, vertex };
      float* data{};
      int size{};
      bool dirty{};
    } main_buffer;
    struct {
      enum { index };
      uint32_t* data{};
      int size{};
      bool dirty{};
    } index_buffer;
  } buffers;

  // This example uses two successive bindings to one buffer.
  struct bindings {
    struct {
      enum { per_vertex };
      int stride = 3 * sizeof(float);
      int step_rate = 1;
    } position_binding;

    struct {
      enum { per_vertex };
      int stride = 2 * sizeof(float);
      int step_rate = 1;
    } texcoord_binding;
  };

  struct attributes {
    struct
    {
      enum { position };
      using datatype = float[3];

      // If we have non-interleaved buffers, e.g. each individual attribute is contiguous
      // offset is 0. Otherwise it's at which point this attribute starts in the buffer data.
      // e.g. if the buffer is [ [ x y z ] [ u v ] [ x y z ] [ u v ] ... ]
      // then the offset for position is 0 and the offset for texcoord is 3 * sizeof(float)
      int32_t offset = 0;
      int32_t binding = 0;
    } position;

    struct {
      enum { tex_coord };
      using datatype = float[2];
      int32_t binding = 1;
    } texcoord;
  };

  struct {
    struct {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int offset = 0;
    } input0;
    struct {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int offset = sizeof(indexed_cube_position);
    } input1;
  } input;

  struct {
    static constexpr auto buffer() { return &buffers::index_buffer; }
    int offset = 0;
  } index;

  // Not vertices !
  int indices = 0;
  bool dirty{};
  enum { triangles, clockwise } ;
};

struct GenerateGeometryNonInterleavedIndexed
{
  static consteval auto name() { return "Generate Geometry"; }
  static consteval auto c_name() { return "avnd_generate_geometry"; }
  static consteval auto uuid() { return "253aa25d-db82-4295-9156-386322573c65"; }

  struct
  {
    struct
    {
      static consteval auto name() { return "Radius"; }
      struct range { double min = 0.001, max = 2, init = 0.1; };
      void update(GenerateGeometryNonInterleavedIndexed& self)
      {
        for(int i = 0; i < std::ssize(indexed_cube_position); i++)
        {
          self.outputs.geometry.buffers.main_buffer.data[i] = self.inputs.radius.value * indexed_cube_position[i];
        }

        self.outputs.geometry.buffers.main_buffer.dirty = true;
        self.outputs.geometry.buffers.index_buffer.dirty = true;
        self.outputs.geometry.dirty = true;
      }

      float value;
    } radius;
  } inputs;


  struct
  {
    indexed_split_geometry geometry;
    static_assert(avnd::geometry_port<decltype(geometry)>);
  } outputs;


  GenerateGeometryNonInterleavedIndexed()
  {
    constexpr auto float_count = std::ssize(indexed_cube_position) + std::ssize(indexed_cube_texcoord);
    outputs.geometry.buffers.main_buffer = {
        .data = new float[float_count],
        .size = float_count,
        .dirty = true
    };
    memcpy(outputs.geometry.buffers.main_buffer.data, indexed_cube_position, sizeof(indexed_cube_position));
    memcpy(outputs.geometry.buffers.main_buffer.data + std::ssize(indexed_cube_position), indexed_cube_texcoord, sizeof(indexed_cube_texcoord));

    constexpr auto index_count = std::ssize(indexed_cube_index);
    outputs.geometry.buffers.index_buffer = {
       .data = new uint32_t[index_count]
     , .size = index_count
     , .dirty= true
    };

    std::copy_n(indexed_cube_index, index_count, outputs.geometry.buffers.index_buffer.data);

    outputs.geometry.input.input0.offset = 0;
    outputs.geometry.input.input1.offset = sizeof(indexed_cube_position);

    outputs.geometry.index.offset = 0;
    outputs.geometry.indices = index_count;
  }

  ~GenerateGeometryNonInterleavedIndexed()
  {
    delete outputs.geometry.buffers.main_buffer.data;
    delete outputs.geometry.buffers.index_buffer.data;
  }

  void operator()() {

  }
};







///////////////////
///////////////////
///////////////////

static constexpr float default_cube_interleaved[] = {
    -1.0f,-1.0f,-1.0f,    0.000059f, 1.0f - 0.000004f,
    -1.0f,-1.0f, 1.0f,    0.000103f, 1.0f - 0.336048f,
    -1.0f, 1.0f, 1.0f,    0.335973f, 1.0f - 0.335903f,
    1.0f, 1.0f, -1.0f,    1.000023f, 1.0f - 0.000013f,
    -1.0f,-1.0f,-1.0f,    0.667979f, 1.0f - 0.335851f,
    -1.0f, 1.0f,-1.0f,    0.999958f, 1.0f - 0.336064f,
    1.0f,-1.0f,  1.0f,    0.667979f, 1.0f - 0.335851f,
    -1.0f,-1.0f,-1.0f,    0.336024f, 1.0f - 0.671877f,
    1.0f,-1.0f, -1.0f,    0.667969f, 1.0f - 0.671889f,
    1.0f, 1.0f, -1.0f,    1.000023f, 1.0f - 0.000013f,
    1.0f,-1.0f, -1.0f,    0.668104f, 1.0f - 0.000013f,
    -1.0f,-1.0f,-1.0f,    0.667979f, 1.0f - 0.335851f,
    -1.0f,-1.0f,-1.0f,    0.000059f, 1.0f - 0.000004f,
    -1.0f, 1.0f, 1.0f,    0.335973f, 1.0f - 0.335903f,
    -1.0f, 1.0f,-1.0f,    0.336098f, 1.0f - 0.000071f,
    1.0f,-1.0f,  1.0f,    0.667979f, 1.0f - 0.335851f,
    -1.0f,-1.0f, 1.0f,    0.335973f, 1.0f - 0.335903f,
    -1.0f,-1.0f,-1.0f,    0.336024f, 1.0f - 0.671877f,
    -1.0f, 1.0f, 1.0f,    1.000004f, 1.0f - 0.671847f,
    -1.0f,-1.0f, 1.0f,    0.999958f, 1.0f - 0.336064f,
    1.0f,-1.0f,  1.0f,    0.667979f, 1.0f - 0.335851f,
    1.0f, 1.0f,  1.0f,    0.668104f, 1.0f - 0.000013f,
    1.0f,-1.0f, -1.0f,    0.335973f, 1.0f - 0.335903f,
    1.0f, 1.0f, -1.0f,    0.667979f, 1.0f - 0.335851f,
    1.0f,-1.0f, -1.0f,    0.335973f, 1.0f - 0.335903f,
    1.0f, 1.0f,  1.0f,    0.668104f, 1.0f - 0.000013f,
    1.0f,-1.0f,  1.0f,    0.336098f, 1.0f - 0.000071f,
    1.0f, 1.0f,  1.0f,    0.000103f, 1.0f - 0.336048f,
    1.0f, 1.0f, -1.0f,    0.000004f, 1.0f - 0.671870f,
    -1.0f, 1.0f,-1.0f,    0.336024f, 1.0f - 0.671877f,
    1.0f, 1.0f,  1.0f,    0.000103f, 1.0f - 0.336048f,
    -1.0f, 1.0f,-1.0f,    0.336024f, 1.0f - 0.671877f,
    -1.0f, 1.0f, 1.0f,    0.335973f, 1.0f - 0.335903f,
    1.0f, 1.0f,  1.0f,    0.667969f, 1.0f - 0.671889f,
    -1.0f, 1.0f, 1.0f,    1.000004f, 1.0f - 0.671847f,
    1.0f,-1.0f,  1.0f,    0.667979f, 1.0f - 0.335851f
};

// In this example the vertex buffer has
// the position and texcoord attributes interleaved
struct non_indexed_interleaved_geometry
{
  static consteval auto name() { return "Geometry"; }

  struct buffers {
    struct {
      enum { dynamic, vertex };
      float* data{};
      int size{};
      bool dirty{};
    } main_buffer;
  } buffers;

  // This example uses a single binding & input
  struct bindings {
    struct {
      enum { per_vertex };
      int stride = 5 * sizeof(float);
      int step_rate = 1;
    } position_texcoord_binding;
  };

  // We still have two attributes
  struct attributes {
    struct
    {
      enum { position };
      using datatype = float[3];
      int32_t offset = 0;
      int32_t binding = 0;
    } position;

    struct {
      enum { tex_coord };
      using datatype = float[2];
      int32_t offset = 3 * sizeof(float);
      int32_t binding = 0;
    } texcoord;
  };

  struct {
    struct {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int offset = 0;
    } input0;
  } input;

  // struct {
  //   static constexpr auto buffer() { return &buffers::main_buffer; }
  //   int offset = 0;
  // } index ;

  int vertices = 0;
  bool dirty{};
  enum { triangles, counter_clockwise, cull_back } ;
};

struct GenerateGeometryInterleaved
{
  static consteval auto name() { return "Generate Geometry"; }
  static consteval auto c_name() { return "avnd_generate_geometry"; }
  static consteval auto uuid() { return "253aa25d-db82-4295-9156-386322573c65"; }

  struct
  {
    struct
    {
      static consteval auto name() { return "Radius"; }
      struct range { double min = 0.001, max = 2, init = 0.1; };
      void update(GenerateGeometryInterleaved& self) {
        constexpr int vertices = std::ssize(default_cube_position) / 3;
        for(int i = 0; i < vertices; i++)
        {
          self.outputs.geometry.buffers.main_buffer.data[i * 5 + 0] = self.inputs.radius.value * default_cube_position[i * 3 + 0];
          self.outputs.geometry.buffers.main_buffer.data[i * 5 + 1] = self.inputs.radius.value * default_cube_position[i * 3 + 1];
          self.outputs.geometry.buffers.main_buffer.data[i * 5 + 2] = self.inputs.radius.value * default_cube_position[i * 3 + 2];
        }

        self.outputs.geometry.buffers.main_buffer.dirty = true;
        self.outputs.geometry.dirty = true;
      }

      float value;
    } radius;
  } inputs;

  struct
  {
    non_indexed_interleaved_geometry geometry;
    static_assert(avnd::geometry_port<decltype(geometry)>);
  } outputs;

  GenerateGeometryInterleaved()
  {
    constexpr auto float_count = std::ssize(default_cube_interleaved);
    outputs.geometry.buffers.main_buffer = {
        .data = new float[float_count],
        .size = float_count,
        .dirty = true
    };
    memcpy(outputs.geometry.buffers.main_buffer.data, default_cube_interleaved, sizeof(default_cube_interleaved));

    outputs.geometry.input.input0.offset = 0;

    outputs.geometry.vertices = std::ssize(default_cube_position) / 3;
  }

  ~GenerateGeometryInterleaved()
  {
    delete outputs.geometry.buffers.main_buffer.data;
  }

  void operator()()
  {
    // Everything happens in the update() of the radius
  }
};

using GenerateGeometry = GenerateGeometryNonInterleaved;
}
