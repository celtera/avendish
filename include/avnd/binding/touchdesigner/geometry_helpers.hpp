#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cstdint>
#include <string_view>

namespace touchdesigner
{

/**
 * Mirror of the halp::attribute_semantic numeric contract (halp/geometry.hpp).
 * The binding does not depend on halp: any geometry type whose attributes
 * carry a `semantic` member following the same numbering works.
 */
enum class attribute_semantic : uint32_t
{
  // Core geometry
  position = 0,
  normal = 1,
  tangent = 2,
  bitangent = 3,

  // Basic materials
  texcoord0 = 100,
  texcoord1 = texcoord0 + 1,
  texcoord2 = texcoord0 + 2,
  texcoord3 = texcoord0 + 3,
  texcoord4 = texcoord0 + 4,
  texcoord5 = texcoord0 + 5,
  texcoord6 = texcoord0 + 6,
  texcoord7 = texcoord0 + 7,

  color0 = 200,
  color1 = color0 + 1,
  color2 = color0 + 2,
  color3 = color0 + 3,

  // Skinning / skeletal animation
  joints0 = 300,
  joints1 = joints0 + 1,

  weights0 = 400,
  weights1 = weights0 + 1,

  // Morph targets / blend shapes
  morph_position = 500,
  morph_normal = morph_position + 1,
  morph_tangent = morph_position + 2,
  morph_texcoord = morph_position + 3,
  morph_color = morph_position + 4,

  // Transform / instancing
  rotation = 600,
  rotation_extra = rotation + 1,
  scale = rotation + 2,
  uniform_scale = rotation + 3,
  up = rotation + 4,
  pivot = rotation + 5,
  transform_matrix = rotation + 6,
  translation = rotation + 7,

  // Particle dynamics
  velocity = 1000,
  acceleration = velocity + 1,
  force = velocity + 2,
  mass = velocity + 3,
  age = velocity + 4,
  lifetime = velocity + 5,
  birth_time = velocity + 6,
  particle_id = velocity + 7,
  drag = velocity + 8,
  angular_velocity = velocity + 9,
  previous_position = velocity + 10,
  rest_position = velocity + 11,
  target_position = velocity + 12,
  previous_velocity = velocity + 13,
  state = velocity + 14,
  collision_count = velocity + 15,
  collision_normal = velocity + 16,
  sleep = velocity + 17,

  // Rendering hints
  sprite_size = 1100,
  sprite_rotation = sprite_size + 1,
  sprite_facing = sprite_size + 2,
  sprite_index = sprite_size + 3,
  width = sprite_size + 4,
  opacity = sprite_size + 5,
  emissive = sprite_size + 6,
  emissive_strength = sprite_size + 7,

  // Material / PBR
  roughness = 1200,
  metallic = roughness + 1,
  ambient_occlusion = roughness + 2,
  specular = roughness + 3,
  subsurface = roughness + 4,
  clearcoat = roughness + 5,
  clearcoat_roughness = roughness + 6,
  anisotropy = roughness + 7,
  anisotropy_direction = roughness + 8,
  ior = roughness + 9,
  transmission = roughness + 10,
  thickness = roughness + 11,
  material_id = roughness + 22,

  // Gaussian splatting
  sh_dc = 1300,
  sh_coeffs = sh_dc + 1,
  covariance_3d = sh_dc + 2,
  sh_degree = sh_dc + 3,

  // Volumetric / field data
  density = 1400,
  temperature = density + 1,
  fuel = density + 2,
  pressure = density + 3,
  divergence = density + 4,
  sdf_distance = density + 5,
  voxel_color = density + 6,

  // Topology / connectivity
  name = 1600,
  piece_id = name + 1,
  line_id = name + 2,
  prim_id = name + 3,
  point_id = name + 4,
  group_mask = name + 5,
  instance_id = name + 6,

  // UI
  selection = 1700,

  // User / general purpose
  fx0 = 2000,
  fx1 = fx0 + 1,
  fx2 = fx0 + 2,
  fx3 = fx0 + 3,
  fx4 = fx0 + 4,
  fx5 = fx0 + 5,
  fx6 = fx0 + 6,
  fx7 = fx0 + 7,

  // Custom (string name lookup)
  custom = 0xFFFF
};

/**
 * Mirror of the halp::attribute_format numeric contract (halp/geometry.hpp).
 */
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

template <typename Attr>
constexpr attribute_semantic get_attribute_semantic(const Attr& attr) noexcept
{
  return static_cast<attribute_semantic>(static_cast<uint32_t>(attr.semantic));
}

template <typename Attr>
constexpr attribute_format get_attribute_format(const Attr& attr) noexcept
{
  return static_cast<attribute_format>(static_cast<uint8_t>(attr.format));
}

constexpr int format_components(attribute_format f) noexcept
{
  switch(f)
  {
    case attribute_format::float4:
    case attribute_format::unormbyte4:
    case attribute_format::uint4:
    case attribute_format::sint4:
    case attribute_format::half4:
    case attribute_format::ushort4:
    case attribute_format::sshort4:
      return 4;
    case attribute_format::float3:
    case attribute_format::uint3:
    case attribute_format::sint3:
    case attribute_format::half3:
    case attribute_format::ushort3:
    case attribute_format::sshort3:
      return 3;
    case attribute_format::float2:
    case attribute_format::unormbyte2:
    case attribute_format::uint2:
    case attribute_format::sint2:
    case attribute_format::half2:
    case attribute_format::ushort2:
    case attribute_format::sshort2:
      return 2;
    case attribute_format::float1:
    case attribute_format::unormbyte1:
    case attribute_format::uint1:
    case attribute_format::sint1:
    case attribute_format::half1:
    case attribute_format::ushort1:
    case attribute_format::sshort1:
      return 1;
  }
  return 0;
}

/**
 * Data kinds that the TD SOP / POP attribute APIs can consume directly:
 * 32-bit floats and 32-bit (un)signed integers. Bytes, halves and shorts
 * would need a conversion pass and are reported as unsupported.
 */
enum class attribute_data_kind : uint8_t
{
  f32,
  u32,
  i32,
  unsupported
};

constexpr attribute_data_kind format_data_kind(attribute_format f) noexcept
{
  switch(f)
  {
    case attribute_format::float4:
    case attribute_format::float3:
    case attribute_format::float2:
    case attribute_format::float1:
      return attribute_data_kind::f32;
    case attribute_format::uint4:
    case attribute_format::uint3:
    case attribute_format::uint2:
    case attribute_format::uint1:
      return attribute_data_kind::u32;
    case attribute_format::sint4:
    case attribute_format::sint3:
    case attribute_format::sint2:
    case attribute_format::sint1:
      return attribute_data_kind::i32;
    default:
      return attribute_data_kind::unsupported;
  }
}

/**
 * Lowercase canonical name for a semantic, used to name custom attributes
 * when the attribute does not carry an explicit name.
 */
constexpr std::string_view semantic_name(attribute_semantic s) noexcept
{
  switch(s)
  {
    case attribute_semantic::position: return "position";
    case attribute_semantic::normal: return "normal";
    case attribute_semantic::tangent: return "tangent";
    case attribute_semantic::bitangent: return "bitangent";
    case attribute_semantic::texcoord0: return "texcoord0";
    case attribute_semantic::texcoord1: return "texcoord1";
    case attribute_semantic::texcoord2: return "texcoord2";
    case attribute_semantic::texcoord3: return "texcoord3";
    case attribute_semantic::texcoord4: return "texcoord4";
    case attribute_semantic::texcoord5: return "texcoord5";
    case attribute_semantic::texcoord6: return "texcoord6";
    case attribute_semantic::texcoord7: return "texcoord7";
    case attribute_semantic::color0: return "color0";
    case attribute_semantic::color1: return "color1";
    case attribute_semantic::color2: return "color2";
    case attribute_semantic::color3: return "color3";
    case attribute_semantic::joints0: return "joints0";
    case attribute_semantic::joints1: return "joints1";
    case attribute_semantic::weights0: return "weights0";
    case attribute_semantic::weights1: return "weights1";
    case attribute_semantic::morph_position: return "morph_position";
    case attribute_semantic::morph_normal: return "morph_normal";
    case attribute_semantic::morph_tangent: return "morph_tangent";
    case attribute_semantic::morph_texcoord: return "morph_texcoord";
    case attribute_semantic::morph_color: return "morph_color";
    case attribute_semantic::rotation: return "rotation";
    case attribute_semantic::rotation_extra: return "rotation_extra";
    case attribute_semantic::scale: return "scale";
    case attribute_semantic::uniform_scale: return "uniform_scale";
    case attribute_semantic::up: return "up";
    case attribute_semantic::pivot: return "pivot";
    case attribute_semantic::transform_matrix: return "transform_matrix";
    case attribute_semantic::translation: return "translation";
    case attribute_semantic::velocity: return "velocity";
    case attribute_semantic::acceleration: return "acceleration";
    case attribute_semantic::force: return "force";
    case attribute_semantic::mass: return "mass";
    case attribute_semantic::age: return "age";
    case attribute_semantic::lifetime: return "lifetime";
    case attribute_semantic::birth_time: return "birth_time";
    case attribute_semantic::particle_id: return "particle_id";
    case attribute_semantic::drag: return "drag";
    case attribute_semantic::angular_velocity: return "angular_velocity";
    case attribute_semantic::previous_position: return "previous_position";
    case attribute_semantic::rest_position: return "rest_position";
    case attribute_semantic::target_position: return "target_position";
    case attribute_semantic::previous_velocity: return "previous_velocity";
    case attribute_semantic::state: return "state";
    case attribute_semantic::collision_count: return "collision_count";
    case attribute_semantic::collision_normal: return "collision_normal";
    case attribute_semantic::sleep: return "sleep";
    case attribute_semantic::sprite_size: return "sprite_size";
    case attribute_semantic::sprite_rotation: return "sprite_rotation";
    case attribute_semantic::sprite_facing: return "sprite_facing";
    case attribute_semantic::sprite_index: return "sprite_index";
    case attribute_semantic::width: return "width";
    case attribute_semantic::opacity: return "opacity";
    case attribute_semantic::emissive: return "emissive";
    case attribute_semantic::emissive_strength: return "emissive_strength";
    case attribute_semantic::roughness: return "roughness";
    case attribute_semantic::metallic: return "metallic";
    case attribute_semantic::ambient_occlusion: return "ambient_occlusion";
    case attribute_semantic::specular: return "specular";
    case attribute_semantic::subsurface: return "subsurface";
    case attribute_semantic::clearcoat: return "clearcoat";
    case attribute_semantic::clearcoat_roughness: return "clearcoat_roughness";
    case attribute_semantic::anisotropy: return "anisotropy";
    case attribute_semantic::anisotropy_direction: return "anisotropy_direction";
    case attribute_semantic::ior: return "ior";
    case attribute_semantic::transmission: return "transmission";
    case attribute_semantic::thickness: return "thickness";
    case attribute_semantic::material_id: return "material_id";
    case attribute_semantic::sh_dc: return "sh_dc";
    case attribute_semantic::sh_coeffs: return "sh_coeffs";
    case attribute_semantic::covariance_3d: return "covariance_3d";
    case attribute_semantic::sh_degree: return "sh_degree";
    case attribute_semantic::density: return "density";
    case attribute_semantic::temperature: return "temperature";
    case attribute_semantic::fuel: return "fuel";
    case attribute_semantic::pressure: return "pressure";
    case attribute_semantic::divergence: return "divergence";
    case attribute_semantic::sdf_distance: return "sdf_distance";
    case attribute_semantic::voxel_color: return "voxel_color";
    case attribute_semantic::name: return "name";
    case attribute_semantic::piece_id: return "piece_id";
    case attribute_semantic::line_id: return "line_id";
    case attribute_semantic::prim_id: return "prim_id";
    case attribute_semantic::point_id: return "point_id";
    case attribute_semantic::group_mask: return "group_mask";
    case attribute_semantic::instance_id: return "instance_id";
    case attribute_semantic::selection: return "selection";
    case attribute_semantic::fx0: return "fx0";
    case attribute_semantic::fx1: return "fx1";
    case attribute_semantic::fx2: return "fx2";
    case attribute_semantic::fx3: return "fx3";
    case attribute_semantic::fx4: return "fx4";
    case attribute_semantic::fx5: return "fx5";
    case attribute_semantic::fx6: return "fx6";
    case attribute_semantic::fx7: return "fx7";
    default: return "custom";
  }
}

/**
 * Conventional POP / Houdini attribute name for a semantic.
 * Returns an empty view when there is no conventional name; callers should
 * then fall back to the attribute's own name or to semantic_name().
 */
constexpr std::string_view pop_attribute_name(attribute_semantic s) noexcept
{
  switch(s)
  {
    case attribute_semantic::position: return "P";
    case attribute_semantic::normal: return "N";
    case attribute_semantic::color0: return "Cd";
    case attribute_semantic::texcoord0: return "uv";
    case attribute_semantic::uniform_scale: return "pscale";
    case attribute_semantic::particle_id: return "id";
    case attribute_semantic::point_id: return "id";
    case attribute_semantic::lifetime: return "life";
    case attribute_semantic::age: return "age";
    case attribute_semantic::velocity: return "v";
    case attribute_semantic::width: return "width";
    case attribute_semantic::rotation: return "orient";
    default: return {};
  }
}

/**
 * Geometry buffers come in two shapes: raw buffers with
 * raw_data / byte_size (e.g. halp::geometry_cpu_buffer) and typed buffers
 * with elements / element_count (e.g. the static halp geometry types).
 */
template <typename Buf>
inline const char* geometry_buffer_data(const Buf& buf) noexcept
{
  if constexpr(requires { buf.raw_data; })
    return static_cast<const char*>(buf.raw_data);
  else if constexpr(requires { buf.elements; })
    return reinterpret_cast<const char*>(buf.elements);
  else
    return nullptr;
}

template <typename Buf>
inline int64_t geometry_buffer_byte_size(const Buf& buf) noexcept
{
  if constexpr(requires { buf.byte_size; })
    return buf.byte_size;
  else if constexpr(requires { buf.element_count; buf.elements[0]; })
    return buf.element_count * int64_t(sizeof(buf.elements[0]));
  else
    return 0;
}

/**
 * Resolves the data pointer and stride of a run-time geometry attribute
 * (dynamic geometry: vectors of buffers / bindings / attributes / inputs,
 *  where input[i] describes the buffer used by binding i).
 */
template <typename Geom, typename Attr>
inline bool resolve_dynamic_attribute(
    Geom& geom, const Attr& attr, const char*& data, int& stride) noexcept
{
  const int binding = attr.binding;
  if(binding < 0 || binding >= int(geom.bindings.size())
     || binding >= int(geom.input.size()))
    return false;

  stride = geom.bindings[binding].stride;

  const auto& input = geom.input[binding];
  if(input.buffer < 0 || input.buffer >= int(geom.buffers.size()))
    return false;

  const char* base = geometry_buffer_data(geom.buffers[input.buffer]);
  if(!base)
    return false;

  data = base + input.byte_offset + attr.byte_offset;
  return true;
}

}
