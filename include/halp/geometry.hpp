#pragma once
#include <halp/modules.hpp>

#include <cinttypes>
#include <memory>
#include <vector>
HALP_MODULE_EXPORT
namespace halp
{
enum class binding_classification : uint8_t
{
  per_vertex,
  per_instance
};
enum class attribute_location : uint32_t
{
  position = 0,
  tex_coord = 1,
  color = 2,
  normal = 3,
  tangent = 4
};
enum class attribute_format : uint8_t
{
  float4,
  float3,
  float2,
  float1,
  unormbyte4,
  unormbyte2,
  unormbyte1,
  uint4,
  uint3,
  uint2,
  uint1,
  sint4,
  sint3,
  sint2,
  sint1,
  half4,
  half3,
  half2,
  half1,
  ushort4,
  ushort3,
  ushort2,
  ushort1,
  sshort4,
  sshort3,
  sshort2,
  sshort1,
};
enum class index_format : uint8_t
{
  uint16,
  uint32
};

enum class primitive_topology : uint8_t
{
  triangles,
  triangle_strip,
  triangle_fan,
  lines,
  line_strip,
  points
};
enum class cull_mode : uint8_t
{
  none,
  front,
  back
};
enum class front_face : uint8_t
{
  counter_clockwise,
  clockwise
};

struct geometry_cpu_buffer
{
  void* raw_data{};
  int64_t byte_size{};
  bool dirty{};
};

struct geometry_gpu_buffer
{
  void* handle{};
  int64_t byte_size{};
  bool dirty{};
};

struct geometry_binding
{
  int stride{};
  int step_rate{};
  halp::binding_classification classification{};
};

struct geometry_attribute
{
  int binding = 0;
  halp::attribute_location location{};
  halp::attribute_format format{};
  int32_t byte_offset{};
};

struct geometry_input
{
  int buffer{}; // Index of the buffer to use
  int64_t byte_offset{};
};

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

// In this example the vertex buffer has only position attributes,
// e.g. for basic xyz point clouds
struct position_geometry
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
      float* elements{};
      int element_count{};
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
      int32_t byte_offset = 0;
      int32_t binding = 0;
    } position;
  };

  struct
  {
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int byte_offset = 0;
    } input0;
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
// strided attributes, e.g. xyzrgbxyzrgb
struct position_color_packed_geometry
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
      float* elements{};
      int element_count{};
      bool dirty{};
    } main_buffer;
  } buffers;

  struct bindings
  {
    struct
    {
      enum
      {
        per_vertex
      };
      int stride = 6 * sizeof(float);
      int step_rate = 1;
    } position_binding;

    struct
    {
      enum
      {
        per_vertex
      };
      int stride = 6 * sizeof(float);
      int step_rate = 1;
    } color_binding;
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
      int32_t byte_offset = 0;
      int32_t binding = 0;
    } position;

    struct
    {
      enum
      {
        color
      };
      using datatype = float[3];
      int32_t byte_offset = 3 * sizeof(float);
      int32_t binding = 0;
    } color;
  };

  struct
  {
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int byte_offset = 0;
    } input0;
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
      float* elements{};
      int element_count{};
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
      int32_t byte_offset = 0;
      int32_t binding = 0;
    } position;

    struct
    {
      enum
      {
        normal
      };
      using datatype = float[3];
      int32_t byte_offset = 0;
      int32_t binding = 1;
    } normal;
  };

  struct
  {
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int byte_offset = 0;
    } input0;
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int byte_offset = 0;
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
      float* elements{};
      int element_count{};
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
      int32_t byte_offset = 0;
      int32_t binding = 0;
    } position;

    struct
    {
      enum
      {
        texcoord
      };
      using datatype = float[2];
      int32_t byte_offset = 0;
      int32_t binding = 1;
    } texcoord;
  };

  struct
  {
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int byte_offset = 0;
    } input0;
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int byte_offset = 0;
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

// In this example the vertex buffer has
// all the position attributes, then all the normal attributes, then all the texcoord attributes
struct position_normals_texcoords_geometry_base
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
      float* elements{};
      int element_count{};
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

    struct
    {
      enum
      {
        per_vertex
      };
      int stride = 2 * sizeof(float);
      int step_rate = 1;
    } uv_binding;
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
      int32_t byte_offset = 0;
      int32_t binding = 0;
    } position;

    struct
    {
      enum
      {
        normal
      };
      using datatype = float[3];
      int32_t byte_offset = 0;
      int32_t binding = 1;
    } normal;
    struct
    {
      enum
      {
        texcoord
      };
      using datatype = float[2];
      int32_t byte_offset = 0;
      int32_t binding = 2;
    } texcoord;
  };

  struct
  {
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int byte_offset = 0;
    } input0;
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int byte_offset = 0; // Offset has to be set correctly by the user at runtime
    } input1;
    struct
    {
      static constexpr auto buffer() { return &buffers::main_buffer; }
      int byte_offset = 0;
    } input2;
  } input;

  int vertices = 0;
};

struct position_normals_texcoords_geometry_plane
    : position_normals_texcoords_geometry_base
{
  enum
  {
    triangles,
    counter_clockwise,
    cull_none
  };
};
struct position_normals_texcoords_geometry_volume
    : position_normals_texcoords_geometry_base
{
  enum
  {
    triangles,
    counter_clockwise,
    cull_back
  };
};


// In this example we have one separate buffer per attribute + an index buffer
// So pretty much everything is defined statically.
struct position_normals_color_index_geometry
{
  enum
  {
    triangles,
    counter_clockwise,
    cull_back
  };

  struct buffers
  {
    struct
    {
      enum
      {
        dynamic,
        vertex
      };
      float* elements{};
      int element_count{};
      bool dirty{};
    } position_buffer;
    struct
    {
      enum
      {
        dynamic,
        vertex
      };
      float* elements{};
      int element_count{};
      bool dirty{};
    } normal_buffer;
    struct
    {
      enum
      {
        dynamic,
        vertex
      };
      float* elements{};
      int element_count{};
      bool dirty{};
    } color_buffer;
    struct
    {
      enum
      {
        dynamic,
        vertex
      };
      uint32_t* elements{};
      int element_count{};
      bool dirty{};
    } index_buffer;
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

    struct
    {
      enum
      {
        per_vertex
      };
      int stride = 4 * sizeof(float);
      int step_rate = 1;
    } color_binding;
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
      int32_t byte_offset = 0;
      int32_t binding = 0;
    } position;

    struct
    {
      enum
      {
        normal
      };
      using datatype = float[3];
      int32_t byte_offset = 0;
      int32_t binding = 1;
    } normal;

    struct
    {
      enum
      {
        color
      };
      using datatype = float[4];
      int32_t byte_offset = 0;
      int32_t binding = 2;
    } texcoord;
  };

  struct
  {
    struct
    {
      static constexpr auto buffer() { return &buffers::position_buffer; }
      static constexpr int byte_offset() { return 0; }
    } input0;
    struct
    {
      static constexpr auto buffer() { return &buffers::normal_buffer; }
      static constexpr int byte_offset() { return 0; }
    } input1;
    struct
    {
      static constexpr auto buffer() { return &buffers::color_buffer; }
      static constexpr int byte_offset() { return 0; }
    } input2;
  } input;

  struct
  {
    static constexpr auto buffer() { return &buffers::index_buffer; }
    static constexpr int byte_offset() { return 0; }
    enum format { uint32 };
  } index;

  int vertices = 0;
};

// Same but no index
struct position_normals_color_geometry
{
  enum
  {
    triangles,
    counter_clockwise,
    cull_back
  };

  struct buffers
  {
    struct
    {
      enum
      {
        dynamic,
        vertex
      };
      float* elements{};
      int element_count{};
      bool dirty{};
    } position_buffer;
    struct
    {
      enum
      {
        dynamic,
        vertex
      };
      float* elements{};
      int element_count{};
      bool dirty{};
    } normal_buffer;
    struct
    {
      enum
      {
        dynamic,
        vertex
      };
      float* elements{};
      int element_count{};
      bool dirty{};
    } color_buffer;
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

    struct
    {
      enum
      {
        per_vertex
      };
      int stride = 4 * sizeof(float);
      int step_rate = 1;
    } color_binding;
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
      int32_t byte_offset = 0;
      int32_t binding = 0;
    } position;

    struct
    {
      enum
      {
        normal
      };
      using datatype = float[3];
      int32_t byte_offset = 0;
      int32_t binding = 1;
    } normal;

    struct
    {
      enum
      {
        color
      };
      using datatype = float[4];
      int32_t byte_offset = 0;
      int32_t binding = 2;
    } texcoord;
  };

  struct
  {
    struct
    {
      static constexpr auto buffer() { return &buffers::position_buffer; }
      static constexpr int byte_offset() { return 0; }
    } input0;
    struct
    {
      static constexpr auto buffer() { return &buffers::normal_buffer; }
      static constexpr int byte_offset() { return 0; }
    } input1;
    struct
    {
      static constexpr auto buffer() { return &buffers::color_buffer; }
      static constexpr int byte_offset() { return 0; }
    } input2;
  } input;

  int vertices = 0;
};

// This example allows to define the geometry at run-time instead

struct dynamic_geometry
{
  std::vector<geometry_cpu_buffer> buffers;
  std::vector<geometry_binding> bindings;
  std::vector<geometry_attribute> attributes;
  std::vector<geometry_input> input;

  int vertices = 0;
  int indices = 0;
  int instances = 1;
  halp::primitive_topology topology{};
  halp::cull_mode cull_mode{};
  halp::front_face front_face{};

  struct
  {
    int buffer{-1};
    int64_t byte_offset{};
    halp::index_format format{};
  } index;
};

// Likewise but with GPU buffers
struct dynamic_gpu_geometry
{
  std::vector<geometry_gpu_buffer> buffers;
  std::vector<geometry_binding> bindings;
  std::vector<geometry_attribute> attributes;
  std::vector<geometry_input> input;

  int vertices = 0;
  int indices = 0;
  int instances = 1;
  halp::primitive_topology topology{};
  halp::cull_mode cull_mode{};
  halp::front_face front_face{};

  struct
  {
    int buffer{-1};
    int64_t byte_offset{};
    halp::index_format format{};
  } index;
};
}
