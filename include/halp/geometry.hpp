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
enum class attribute_semantic : uint32_t
{
  // Core geometry
  position = 0,  // vec3.  Object-space position.
  normal = 1,    // vec3.  Surface normal.
  tangent = 2,   // vec4.  xyz=tangent, w=handedness (±1). [glTF TANGENT]
  bitangent = 3, // vec3.  cross(N, T.xyz) * T.w.

  // Basic materials
  texcoord0 = 100,           // vec2/3. Primary UV. [glTF TEXCOORD_0]
  texcoord1 = texcoord0 + 1, // vec2. Secondary UV (lightmaps). [glTF TEXCOORD_1]
  texcoord2 = texcoord0 + 2, // vec2. Secondary UV. [glTF TEXCOORD_2]
  texcoord3 = texcoord0 + 3, // vec2. Secondary UV. [glTF TEXCOORD_3]
  texcoord4 = texcoord0 + 4, // vec2. Secondary UV. [glTF TEXCOORD_4]
  texcoord5 = texcoord0 + 5, // vec2. Secondary UV.
  texcoord6 = texcoord0 + 6, // vec2. Secondary UV.
  texcoord7 = texcoord0 + 7, // vec2. Secondary UV.

  color0 = 200,        // vec4.  Vertex color RGBA. [glTF COLOR_0]
  color1 = color0 + 1, // vec4.  Secondary vertex color. [glTF COLOR_1]
  color2 = color0 + 2, // vec4.  Secondary vertex color.
  color3 = color0 + 3, // vec4.  Secondary vertex color.

  // Skinning / skeletal animation
  joints0 = 300,         // uvec4. Bone indices, set 0. [glTF JOINTS_0]
  joints1 = joints0 + 1, // uvec4. Bone indices, set 1. [glTF JOINTS_1]

  weights0 = 400,          // vec4.  Bone weights, set 0. [glTF WEIGHTS_0]
  weights1 = weights0 + 1, // vec4.  Bone weights, set 1. [glTF WEIGHTS_1]

  // Morph targets / blend shapes
  morph_position = 500,              // vec3.  Position delta for morph target.
  morph_normal = morph_position + 1, // vec3.  Normal delta for morph target.
  morph_tangent
  = morph_position + 2, // vec3.  Tangent delta (no w). [glTF morph TANGENT]
  morph_texcoord = morph_position + 3, // vec2.  UV delta for morph target.
  morph_color = morph_position + 4,    // vec3/4. Color delta for morph target.

  // Transform / instancing
  rotation = 600,                      // vec4.  Quaternion (x,y,z,w).
  rotation_extra = morph_position + 1, // vec4.  Post-orient rotation.
  scale = morph_position + 2,          // vec3.  Non-uniform scale.
  uniform_scale = morph_position + 3,  // float. Uniform scale.
  up = morph_position + 4,             // vec3.  Up vector for LookAt.
  pivot = morph_position + 5,          // vec3.  Local pivot point.
  transform_matrix
  = morph_position
    + 6, // mat4.  Full transform, overrides TRS. (note: remember that mat4 takes 4 lanes of attributes)
  translation = morph_position + 7, // vec3.  Additional translation offset.

  // Particle dynamics
  velocity = 1000,                   // vec3.  Velocity in units/sec.
  acceleration = velocity + 1,       // vec3.  Current acceleration.
  force = velocity + 2,              // vec3.  Accumulated force this frame.
  mass = velocity + 3,               // float.
  age = velocity + 4,                // float. Time since birth, seconds.
  lifetime = velocity + 5,           // float. Max age before death.
  birth_time = velocity + 6,         // float. Absolute time of birth.
  particle_id = velocity + 7,        // int.   Stable unique ID.
  drag = velocity + 8,               // float. Per-particle drag coefficient.
  angular_velocity = velocity + 9,   // vec3.  Rotation speed rad/sec.
  previous_position = velocity + 10, // vec3.  For Verlet / motion blur.
  rest_position = velocity + 11,     // vec3.  Undeformed position.
  target_position = velocity + 12,   // vec3.  Goal position for constraints.
  previous_velocity = velocity + 13, // vec3.  Velocity at previous frame.
  state = velocity + 14,             // int.   alive/dying/dead/collided enum.
  collision_count = velocity + 15,   // int.   Number of collisions.
  collision_normal = velocity + 16,  // vec3.  Normal at last collision.
  sleep = velocity + 17,             // int.   Dormant flag (skip simulation).

  // Rendering hints
  sprite_size = 1100,                  // vec2.  Billboard width/height.
  sprite_rotation = sprite_size + 1,   // float. Billboard screen-space rotation.
  sprite_facing = sprite_size + 2,     // vec3.  Custom billboard facing direction.
  sprite_index = sprite_size + 3,      // int/float. Sub-image index for sprite sheets.
  width = sprite_size + 4,             // float. Curve/ribbon thickness.
  opacity = sprite_size + 5,           // float. Separate from color alpha.
  emissive = sprite_size + 6,          // vec3.  Self-illumination color.
  emissive_strength = sprite_size + 7, // float. Emissive intensity multiplier.

  // Material / PBR
  roughness = 1200,                     // float. PBR roughness [0-1].
  metallic = roughness + 1,             // float. PBR metalness [0-1].
  ambient_occlusion = roughness + 2,    // float. Baked AO [0-1].
  specular = roughness + 3,             // float. Specular factor.
  subsurface = roughness + 4,           // float. SSS intensity.
  clearcoat = roughness + 5,            // float. Clearcoat factor.
  clearcoat_roughness = roughness + 6,  // float. Clearcoat roughness.
  anisotropy = roughness + 7,           // float. Anisotropic reflection.
  anisotropy_direction = roughness + 8, // vec3.  Anisotropy tangent direction.
  ior = roughness + 9,                  // float. Index of refraction.
  transmission = roughness + 10,        // float. Transmission factor (glass-like).
  thickness = roughness + 11,           // float. Volume thickness for transmission.
  material_id = roughness + 22,         // int.   Index into material array.

  // Gaussian splatting
  sh_dc = 1300,              // vec3.  SH degree-0 (DC) color.
  sh_coeffs = sh_dc + 1,     // float[N]. SH coefficients for higher degrees.
  covariance_3d = sh_dc + 2, // vec6 or mat3. 3D covariance (6 unique floats).
  sh_degree = sh_dc + 3,     // int.   Active SH degree for this splat (0-3).

  // Volumetric / field data
  density = 1400,             // float. Scalar density.
  temperature = density + 1,  // float.
  fuel = density + 2,         // float.
  pressure = density + 3,     // float.
  divergence = density + 4,   // float.
  sdf_distance = density + 5, // float. Signed distance field value.
  voxel_color = density + 6,  // vec4. Per-voxel RGBA.

  // Topology / connectivity
  name = 1600,            // string. Piece/group identifier.
  piece_id = name + 1,    // int.   Numeric piece/group index.
  line_id = name + 2,     // int.   Which line strip this point belongs to.
  prim_id = name + 3,     // int.   Source primitive index.
  point_id = name + 4,    // int.   Stable point ID (distinct from array index).
  group_mask = name + 5,  // int.   Bitfield for group membership.
  instance_id = name + 6, // int.   Which instance this element belongs to.

  // UI
  selection = 1700, // float. Soft selection weight [0-1].

  // User / general purpose
  fx0 = 2000,    // float. General-purpose effect control.
  fx1 = fx0 + 1, // float. General-purpose effect control.
  fx2 = fx0 + 2, // float. General-purpose effect control.
  fx3 = fx0 + 3, // float. General-purpose effect control.
  fx4 = fx0 + 4, // float. General-purpose effect control.
  fx5 = fx0 + 5, // float. General-purpose effect control.
  fx6 = fx0 + 6, // float. General-purpose effect control.
  fx7 = fx0 + 7, // float. General-purpose effect control.

  // Custom (string name lookup)
  custom = 0xFFFF
  //   position = 0,
  //   tex_coord = 1,
  //   color = 2,
  //   normal = 3,
  //   tangent = 4
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
  halp::attribute_semantic semantic{};
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
