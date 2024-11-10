#pragma once
#include <halp/modules.hpp>

#include <cinttypes>
#include <memory>
#include <vector>
HALP_MODULE_EXPORT
namespace halp
{

template <typename T>
struct rect2d
{
  T x{}, y{}, w{}, h{};
};

struct mesh
{
  float transform[16]{};
  bool dirty_mesh = false;
  bool dirty_transform = false;
};

// In this example the vertex buffer has
// all the position attributes, then all the normal attributes
struct position_normals_geometry
{
  struct buffers
  {
    struct
    {
      enum
      {
        dynamic,
        vertex
      };
      float* data{};
      int size{};
      bool dirty{};
    } main_buffer;
  } buffers;

  // This example uses two successive bindings to one buffer.
  struct bindings
  {
    struct
    {
      enum
      {
        per_vertex
      };
      int stride = 3 * sizeof(float);
      int step_rate = 1;
    } position_binding;

    struct
    {
      enum
      {
        per_vertex
      };
      int stride = 3 * sizeof(float);
      int step_rate = 1;
    } normals_binding;
  };

  struct attributes
  {
    struct
    {
      enum
      {
        position
      };
      using datatype = float[3];
      int32_t offset = 0;
      int32_t binding = 0;
    } position;

    struct
    {
      enum
      {
        normal
      };
      using datatype = float[3];
      int32_t offset = 0;
      int32_t binding = 1;
    } normal;
  };

  struct
  {
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int offset = 0;
    } input0;
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int offset = 0;
    } input1;
  } input;

  int vertices = 0;
  enum
  {
    triangles,
    counter_clockwise,
    cull_back
  };
};

// In this example the vertex buffer has
// all the position attributes, then all the texcoord attributes
struct position_texcoords_geometry
{
  struct buffers
  {
    struct
    {
      enum
      {
        dynamic,
        vertex
      };
      float* data{};
      int size{};
      bool dirty{};
    } main_buffer;
  } buffers;

  // This example uses two successive bindings to one buffer.
  struct bindings
  {
    struct
    {
      enum
      {
        per_vertex
      };
      int stride = 3 * sizeof(float);
      int step_rate = 1;
    } position_binding;

    struct
    {
      enum
      {
        per_vertex
      };
      int stride = 2 * sizeof(float);
      int step_rate = 1;
    } texcoord_binding;
  };

  struct attributes
  {
    struct
    {
      enum
      {
        position
      };
      using datatype = float[3];
      int32_t offset = 0;
      int32_t binding = 0;
    } position;

    struct
    {
      enum
      {
        texcoord
      };
      using datatype = float[2];
      int32_t offset = 0;
      int32_t binding = 1;
    } texcoord;
  };

  struct
  {
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int offset = 0;
    } input0;
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int offset = 0;
    } input1;
  } input;

  int vertices = 0;
  enum
  {
    triangle_strip,
    counter_clockwise,
    cull_back
  };
};

// This example allows to define the geometry at run-time instead

struct dynamic_geometry
{
  struct buffer
  {
    void* data{};
    int64_t size{};
    bool dirty{};
  };

  struct binding
  {
    int stride{};
    int step_rate{};
    enum
    {
      per_vertex,
      per_instance
    } classification{};
  };

  struct attribute
  {
    int binding = 0;
    enum : uint32_t
    {
      position = 0,
      tex_coord = 1,
      color = 2,
      normal = 3,
      tangent = 4
    } location{};
    enum
    {
      float4,
      float3,
      float2,
      float1,
      uint4,
      uint2,
      uint1
    } format{};
    int32_t offset{};
  };

  struct input
  {
    int buffer{}; // Index of the buffer to use
    int64_t offset{};
  };

  std::vector<buffer> buffers;
  std::vector<binding> bindings;
  std::vector<attribute> attributes;
  std::vector<input> input;
  int vertices = 0;
  enum
  {
    triangles,
    triangle_strip,
    triangle_fan,
    lines,
    line_strip,
    points
  } topology;
  enum
  {
    none,
    front,
    back
  } cull_mode;
  enum
  {
    counter_clockwise,
    clockwise
  } front_face;

  struct
  {
    int buffer{-1};
    int64_t offset{};
    enum
    {
      uint16,
      uint32
    } format{};
  } index;
};

}
