#pragma once

#include <avnd/common/enums.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/concepts/gfx.hpp>
#include <halp/polyfill.hpp>
#include <ossia/dataflow/geometry_port.hpp>

#include <memory>

namespace oscr
{

template <typename T>
struct is_shared_ptr : std::false_type
{
};
template <typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type
{
};

template <typename Attr>
constexpr int standard_location_for_attribute(int k)
{
  if constexpr(requires { Attr::position; } || requires { Attr::positions; })
    return 0;
  else if constexpr(requires { Attr::texcoord; } || requires {
                      Attr::tex_coord;
                    } || requires { Attr::texcoords; } || requires {
                      Attr::tex_coords;
                    } || requires { Attr::uv; } || requires { Attr::uvs; })
    return 1;
  else if constexpr(requires { Attr::color; } || requires { Attr::colors; })
    return 2;
  else if constexpr(requires { Attr::normal; } || requires { Attr::normals; })
    return 3;
  else if constexpr(requires { Attr::tangent; } || requires { Attr::tangents; })
    return 4;
  else
    return k;
}

template <typename T>
constexpr auto get_topology(const T& t) -> decltype(ossia::geometry::topology)
{
  static_constexpr auto m = AVND_ENUM_OR_TAG_MATCHER(
      topology, (ossia::geometry::triangles, triangle, triangles),
      (ossia::geometry::triangle_strip, triangle_strip, triangles_strip),
      (ossia::geometry::triangle_fan, triangle_fan, triangles_fan),
      (ossia::geometry::lines, line, lines), (ossia::geometry::points, point, points));
  return m(t, ossia::geometry::triangles);
}

template <typename T>
constexpr auto get_front_face(const T& t) -> decltype(ossia::geometry::front_face)
{
  static_constexpr auto m = AVND_ENUM_OR_TAG_MATCHER(
      front_face, (ossia::geometry::clockwise, clockwise, cw, CW),
      (ossia::geometry::counter_clockwise, counter_clockwise, ccw, CCW));
  return m(t, ossia::geometry::clockwise);
}

template <typename T>
constexpr auto get_cull_mode(const T& t) -> decltype(ossia::geometry::cull_mode)
{
  static_constexpr auto m = AVND_ENUM_OR_TAG_MATCHER(
      cull_mode, (ossia::geometry::front, front, cull_front),
      (ossia::geometry::back, back, cull_back), (ossia::geometry::none, none));
  return m(t, ossia::geometry::none);
}

/// avnd geometry output -> ossia geometry
template <typename T>
void load_buffer(T& buf, std::size_t buf_k, ossia::geometry& geom) = delete;

template <avnd::geometry_cpu_buffer T>
void load_buffer(T& buf, std::size_t buf_k, ossia::geometry& geom)
{
  // FIXME check if offset handling is required here ?
  using ptr_type = std::decay_t<decltype(buf.raw_data)>;
  static_assert(std::is_same_v<ptr_type, void*>);
  // using data_type = float[4];
  // Dynamic case
  if(buf_k >= geom.buffers.size())
  {
    ossia::geometry::cpu_buffer cpu_buf;
    auto data = std::make_unique<char[]>(buf.byte_size);
    std::copy_n((char*)buf.raw_data, buf.byte_size, data.get());
    cpu_buf.raw_data = std::move(data);
    cpu_buf.byte_size = buf.byte_size;
    // cpu_buf.byte_offset = buf.byte_offset; // untyped buffer offset is always 0
    ossia::geometry::buffer in{.data = std::move(cpu_buf), .dirty = true};
    buf.dirty = false;
    in.dirty = true;
    geom.buffers.push_back(std::move(in));
  }
  else if(buf.dirty)
  {
    ossia::geometry::cpu_buffer cpu_buf;
    auto data = std::make_unique<char[]>(buf.byte_size);
    std::copy_n((char*)buf.raw_data, buf.byte_size, data.get());
    cpu_buf.raw_data = std::move(data);
    cpu_buf.byte_size = buf.byte_size;
    // cpu_buf.byte_offset = buf.byte_offset;

    // FIXME recycle old in.data
    auto& in = geom.buffers[buf_k];
    in.data = std::move(cpu_buf);
    buf.dirty = false;
    in.dirty = true;
  }
}

template <avnd::geometry_cpu_typed_buffer T>
void load_buffer(T& buf, std::size_t buf_k, ossia::geometry& geom)
{
  // FIXME check if offset handling is required here ?
  using ptr_type = std::decay_t<decltype(buf.elements)>;
  using data_type = std::decay_t<decltype(buf.elements[0])>;
  static_assert(!std::is_same_v<ptr_type, void*>);
  static_assert(sizeof(data_type) > 1);
  static_assert(!std::is_reference_v<data_type>);
  static_assert(!std::is_const_v<data_type>);

  // Static case
  if(buf_k >= geom.buffers.size())
  {
    ossia::geometry::cpu_buffer cpu_buf;
    // FIXME find a way to do it with some linear allocator or whatever
    // - we need to handle cleanly both static and dynamic geometry
    // if constexpr(is_shared_ptr<ptr_type>::value)
    // {
    //   cpu_buf.data = buf.data;
    // }
    // else
    {
      auto data = std::make_unique<data_type[]>(buf.element_count);
      std::copy_n(buf.elements, buf.element_count, data.get());
      cpu_buf.raw_data = std::move(data);
      cpu_buf.byte_size = buf.element_count * sizeof(data_type);
      // cpu_buf.byte_offset = buf.byte_offset;
    }

    ossia::geometry::buffer in{.data = std::move(cpu_buf), .dirty = true};
    buf.dirty = false;

    geom.buffers.push_back(std::move(in));
  }
  else if(buf.dirty)
  {
    ossia::geometry::cpu_buffer cpu_buf;
    auto data = std::make_unique<data_type[]>(buf.element_count);
    std::copy_n(buf.elements, buf.element_count, data.get());
    cpu_buf.raw_data = std::move(data);
    cpu_buf.byte_size = buf.element_count * sizeof(data_type);
    // cpu_buf.byte_offset = buf.byte_offset;

    // FIXME recycle old in.data
    auto& in = geom.buffers[buf_k];
    in.data = std::move(cpu_buf);
    buf.dirty = false;
    in.dirty = true;
  }
}

template <avnd::geometry_gpu_buffer T>
void load_buffer(T& buf, std::size_t buf_k, ossia::geometry& geom)
{
  // Dynamic case
  if(buf_k >= geom.buffers.size())
  {
    ossia::geometry::gpu_buffer gpu_buf;
    gpu_buf.handle = buf.handle;
    gpu_buf.byte_size = buf.byte_size;
    // gpu_buf.byte_offset = buf.byte_offset;

    ossia::geometry::buffer in{.data = std::move(gpu_buf), .dirty = true};
    buf.dirty = false;
    in.dirty = true;
    geom.buffers.push_back(std::move(in));
  }
  else
  {
    ossia::geometry::gpu_buffer gpu_buf;
    gpu_buf.handle = buf.handle;
    gpu_buf.byte_size = buf.byte_size;
    // gpu_buf.byte_offset = buf.byte_offset;

    // FIXME recycle old in.data
    auto& in = geom.buffers[buf_k];
    in.data = gpu_buf;
    buf.dirty = false;
    in.dirty = true;
  }
}

template <typename T>
  requires(avnd::static_geometry_type<T> || avnd::dynamic_geometry_type<T>)
void load_geometry(T& ctrl, ossia::geometry& geom)
{
  if constexpr(requires { ctrl.vertices; })
    geom.vertices = ctrl.vertices;
  if constexpr(requires { ctrl.indices; })
    geom.indices = ctrl.indices;
  if constexpr(requires { ctrl.instances; })
    geom.instances = ctrl.instances;

  geom.topology = get_topology(ctrl);
  geom.front_face = get_front_face(ctrl);
  geom.cull_mode = get_cull_mode(ctrl);

  avnd::for_each_field_ref_n(ctrl.buffers, [&geom](auto& buf, std::size_t buf_k) {
    load_buffer(buf, buf_k, geom);
  });

  geom.input.clear();
  avnd::for_each_field_ref(ctrl.input, [&](auto& input) {
    struct ossia::geometry::input in;

    if constexpr(requires { input.buffer(); })
      in.buffer = avnd::index_in_struct(ctrl.buffers, input.buffer());
    else
      in.buffer = input.buffer;

    in.byte_offset = input.byte_offset;

    geom.input.push_back(in);
  });

  if constexpr(requires { ctrl.index; })
  {
    // Index buffer setup
    if constexpr(requires { ctrl.index.buffer(); })
    {
      geom.index.buffer = avnd::index_in_struct(ctrl.buffers, ctrl.index.buffer());
      using index_buf_type
          = std::decay_t<decltype((ctrl.buffers.*(ctrl.index.buffer())).data[0])>;

      switch(sizeof(index_buf_type))
      {
        case 2:
          geom.index.format = decltype(ossia::geometry::index)::uint16;
          break;
        case 4:
          geom.index.format = decltype(ossia::geometry::index)::uint32;
          break;
        default:
          assert(false);
          break;
      }
    }
    else if constexpr(requires { ctrl.index.buffer; })
    {
      geom.index.buffer = ctrl.index.buffer;
      if(ctrl.index.format == decltype(ctrl.index.format)::uint16)
        geom.index.format = decltype(ossia::geometry::index)::uint16;
      else if(ctrl.index.format == decltype(ctrl.index.format)::uint32)
        geom.index.format = decltype(ossia::geometry::index)::uint32;
    }
    else
    {
      geom.index.buffer = -1;
    }

    if constexpr(requires { ctrl.index.byte_offset(); })
      geom.index.byte_offset = ctrl.index.byte_offset();
    else if constexpr(requires { ctrl.index.byte_offset; })
      geom.index.byte_offset = ctrl.index.byte_offset;
    else
      geom.index.byte_offset = 0;
  }

  if(geom.bindings.empty())
  {
    avnd::for_each_field_ref(avnd::get_bindings(ctrl), [&](auto& binding) {
      ossia::geometry::binding b;

      b.byte_stride = binding.stride;
      if constexpr(requires { binding.classification; })
      {
        static_constexpr auto m = AVND_ENUM_OR_TAG_MATCHER(
            classification, (ossia::geometry::binding::per_vertex, per_vertex),
            (ossia::geometry::binding::per_instance, per_instance));
        b.classification = m(binding, ossia::geometry::binding::per_vertex);
      }

      if constexpr(requires { binding.step_rate; })
        b.step_rate = binding.step_rate;

      geom.bindings.push_back(b);
    });
  }

  if(geom.attributes.empty())
  {
    std::size_t k = 0;
    avnd::for_each_field_ref(avnd::get_attributes(ctrl), [&]<typename A>(A& attr) {
      ossia::geometry::attribute a;

      // FIXME very brittle
      if constexpr(requires { a.location = static_cast<int>(attr.location); })
        a.location = static_cast<int>(attr.location);
      else
        a.location = standard_location_for_attribute<A>(k++);

      if constexpr(requires { attr.binding; })
        a.binding = attr.binding;

      if constexpr(requires { attr.byte_offset; })
        a.byte_offset = attr.byte_offset;

      if constexpr(requires { typename A::datatype; })
      {
        using tp = typename A::datatype;
        if constexpr(std::is_same_v<tp, float>)
          a.format = ossia::geometry::attribute::float1;
        else if constexpr(std::is_same_v<tp, float[2]>)
          a.format = ossia::geometry::attribute::float2;
        else if constexpr(std::is_same_v<tp, float[3]>)
          a.format = ossia::geometry::attribute::float3;
        else if constexpr(std::is_same_v<tp, float[4]>)
          a.format = ossia::geometry::attribute::float4;

        else if constexpr(
            std::is_same_v<tp, uint8_t> || std::is_same_v<tp, unsigned char>)
          a.format = ossia::geometry::attribute::unormbyte1;
        else if constexpr(
            std::is_same_v<tp, uint8_t[2]> || std::is_same_v<tp, unsigned char[2]>)
          a.format = ossia::geometry::attribute::unormbyte2;
        else if constexpr(
            std::is_same_v<tp, uint8_t[4]> || std::is_same_v<tp, unsigned char[4]>)
          a.format = ossia::geometry::attribute::unormbyte4;

        else if constexpr(std::is_same_v<tp, uint32_t>)
          a.format = ossia::geometry::attribute::uint1;
        else if constexpr(std::is_same_v<tp, uint32_t[2]>)
          a.format = ossia::geometry::attribute::uint2;
        else if constexpr(std::is_same_v<tp, uint32_t[3]>)
          a.format = ossia::geometry::attribute::uint3;
        else if constexpr(std::is_same_v<tp, uint32_t[4]>)
          a.format = ossia::geometry::attribute::uint4;

        else if constexpr(std::is_same_v<tp, int32_t>)
          a.format = ossia::geometry::attribute::sint1;
        else if constexpr(std::is_same_v<tp, int32_t[2]>)
          a.format = ossia::geometry::attribute::sint2;
        else if constexpr(std::is_same_v<tp, int32_t[3]>)
          a.format = ossia::geometry::attribute::sint3;
        else if constexpr(std::is_same_v<tp, int32_t[4]>)
          a.format = ossia::geometry::attribute::sint4;

#if defined(__STDCPP_FLOAT16_T__)
        else if constexpr(std::is_same_v<tp, _Float16>)
          a.format = ossia::geometry::attribute::half1;
        else if constexpr(std::is_same_v<tp, _Float16[2]>)
          a.format = ossia::geometry::attribute::half2;
        else if constexpr(std::is_same_v<tp, _Float16[3]>)
          a.format = ossia::geometry::attribute::half3;
        else if constexpr(std::is_same_v<tp, _Float16[4]>)
          a.format = ossia::geometry::attribute::half4;
#endif

        else if constexpr(std::is_same_v<tp, uint16_t>)
          a.format = ossia::geometry::attribute::ushort1;
        else if constexpr(std::is_same_v<tp, uint16_t[2]>)
          a.format = ossia::geometry::attribute::ushort2;
        else if constexpr(std::is_same_v<tp, uint16_t[3]>)
          a.format = ossia::geometry::attribute::ushort3;
        else if constexpr(std::is_same_v<tp, uint16_t[4]>)
          a.format = ossia::geometry::attribute::ushort4;

        else if constexpr(std::is_same_v<tp, int16_t>)
          a.format = ossia::geometry::attribute::sshort1;
        else if constexpr(std::is_same_v<tp, int16_t[2]>)
          a.format = ossia::geometry::attribute::sshort2;
        else if constexpr(std::is_same_v<tp, int16_t[3]>)
          a.format = ossia::geometry::attribute::sshort3;
        else if constexpr(std::is_same_v<tp, int16_t[4]>)
          a.format = ossia::geometry::attribute::sshort4;
      }
      else
      {
        static_constexpr auto m = AVND_ENUM_MATCHER(
            (ossia::geometry::attribute::float4, float4, fp4, Float4, vec4),
            (ossia::geometry::attribute::float3, float3, fp3, Float3, vec3),
            (ossia::geometry::attribute::float2, float2, fp2, Float2, vec2),
            (ossia::geometry::attribute::float1, float1, fp1, Float1),
            (ossia::geometry::attribute::unormbyte4, unormbyte4, uchar4),
            (ossia::geometry::attribute::unormbyte2, unormbyte2, uchar2),
            (ossia::geometry::attribute::unormbyte1, unormbyte1, uchar),
            (ossia::geometry::attribute::uint4, uint4, unsigned4, u4, uvec4),
            (ossia::geometry::attribute::uint3, uint3, unsigned3, u3, uvec3),
            (ossia::geometry::attribute::uint2, uint2, unsigned2, u2, uvec2),
            (ossia::geometry::attribute::uint1, uint1, unsigned1, u1),
            (ossia::geometry::attribute::sint4, sint4, signed4, i4, s4, ivec4),
            (ossia::geometry::attribute::sint3, sint3, signed3, i3, s3, ivec3),
            (ossia::geometry::attribute::sint2, sint2, signed2, i2, s2, ivec2),
            (ossia::geometry::attribute::sint1, sint1, signed1, i1, s1),
            (ossia::geometry::attribute::ushort4, ushort4),
            (ossia::geometry::attribute::ushort3, ushort3),
            (ossia::geometry::attribute::ushort2, ushort2),
            (ossia::geometry::attribute::ushort1, ushort1),
            (ossia::geometry::attribute::sshort4, sshort4, ishort4),
            (ossia::geometry::attribute::sshort3, sshort3, ishort3),
            (ossia::geometry::attribute::sshort2, sshort2, ishort2),
            (ossia::geometry::attribute::sshort1, sshort1, ishort1));
        a.format = m(attr.format, ossia::geometry::attribute::float4);
      }

      geom.attributes.push_back(a);
    });
  }
}

template <typename T>
  requires(
      avnd::static_geometry_type<typename decltype(T::mesh)::value_type>
      || avnd::dynamic_geometry_type<typename decltype(T::mesh)::value_type>)
void load_geometry(T& ctrl, ossia::mesh_list& geom)
{
  const auto N = ctrl.mesh.size();
  geom.meshes.resize(N);
  for(int i = 0; i < N; i++)
  {
    load_geometry(ctrl.mesh[i], geom.meshes[i]);
  }
  geom.dirty_index++;
}

struct geometry_dirty_check_result
{
  bool need_reload = false;
  bool need_upload = false;
};
// Returns true if dirty
template <avnd::geometry_cpu_buffer T>
[[nodiscard]] geometry_dirty_check_result
update_buffer(T& buf, std::size_t buf_k, ossia::geometry& geom)
{
  auto& in = geom.buffers[buf_k];
  if(buf.dirty)
  {
    if(auto cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&in.data))
    {
      using ptr_type = std::decay_t<decltype(buf.raw_data)>;
      static_assert(std::is_same_v<ptr_type, void*>);
      if(cpu->byte_size == buf.byte_size)
      {
        std::memcpy(cpu->raw_data.get(), buf.raw_data, buf.byte_size);
        buf.dirty = false;
        in.dirty = true;
        return {false, true};
      }
      else
      {
        in.dirty = true;
        return {true, true};
      }
    }
    else if(auto gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&in.data))
    {
      // We have a dirty cpu buffer and we need to upload it into a gpu buffer
      buf.dirty = false;
      in.dirty = true;
      return {true, true};
    }
    else
    {
      in.dirty = true;
      return {true, true};
    }
  }
  else
  {
    in.dirty = false;
  }
  return {false, false};
}

template <avnd::geometry_cpu_typed_buffer T>
[[nodiscard]] geometry_dirty_check_result
update_buffer(T& buf, std::size_t buf_k, ossia::geometry& geom)
{
  auto& in = geom.buffers[buf_k];
  if(buf.dirty)
  {
    if(auto cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&in.data))
    {
      using ptr_type = std::decay_t<decltype(buf.elements)>;
      using data_type = std::decay_t<decltype(buf.elements[0])>;
      if(cpu->byte_size == buf.element_count * sizeof(data_type))
      {
        std::memcpy(
            cpu->raw_data.get(), buf.elements, buf.element_count * sizeof(data_type));

        buf.dirty = false;
        in.dirty = true;
        return {false, true};
      }
      else
      {
        buf.dirty = false;
        in.dirty = true;
        return {true, true};
      }
    }
    else if(auto gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&in.data))
    {
      // We have a dirty cpu buffer and we need to upload it into a gpu buffer
      buf.dirty = false;
      in.dirty = true;
      return {true, true};
    }
    else
    {
      buf.dirty = false;
      in.dirty = true;
      return {true, true};
    }
  }
  else
  {
    in.dirty = false;
  }
  return {false, false};
}

template <avnd::geometry_gpu_buffer T>
[[nodiscard]] geometry_dirty_check_result
update_buffer(T& buf, std::size_t buf_k, ossia::geometry& geom)
{
  // We have a gpu buffer, we replace the CPU buffer in any case
  ossia::geometry::gpu_buffer gpu_buf;
  gpu_buf.handle = buf.handle;
  gpu_buf.byte_size = buf.byte_size;

  SCORE_ASSERT(buf_k >= 0);
  SCORE_ASSERT(buf_k < geom.buffers.size());
  auto& in = geom.buffers[buf_k];
  in.data = gpu_buf;

  if(buf.dirty)
  {
    buf.dirty = false;
    in.dirty = true;
    return {false, true};
  }
  else
  {
    in.dirty = false;
    return {false, false};
  }
}

template <typename T>
  requires(avnd::static_geometry_type<T> || avnd::dynamic_geometry_type<T>)
[[nodiscard]] geometry_dirty_check_result update_geometry(T& ctrl, ossia::geometry& geom)
{
  // return true -> needs a reload
  if constexpr(requires { ctrl.vertices; })
    if(geom.vertices != ctrl.vertices)
      return {true, false};
  if constexpr(requires { ctrl.indices; })
    if(geom.indices != ctrl.indices)
      return {true, false};
  if constexpr(requires { ctrl.instances; })
    if(geom.instances != ctrl.instances)
      return {true, false};

  geometry_dirty_check_result r{false, false};
  avnd::for_each_field_ref_n(ctrl.buffers, [&](auto& buf, std::size_t buf_k) {
    if(r.need_reload)
      return;
    if(buf_k >= geom.buffers.size())
    {
      r.need_reload = true;
      return;
    }

    auto ret = update_buffer(buf, buf_k, geom);
    r.need_reload |= ret.need_reload;
    r.need_upload |= ret.need_upload;
  });
  return r;
}

template <typename T>
  requires(
      avnd::static_geometry_type<typename decltype(T::mesh)::value_type>
      || avnd::dynamic_geometry_type<typename decltype(T::mesh)::value_type>)
geometry_dirty_check_result update_geometry(T& ctrl, ossia::mesh_list& geom)
{
  const auto N = ctrl.mesh.size();
  if(N != geom.meshes.size())
    return {true, true};

  bool any_dirty = false;
  for(int i = 0; i < N; i++)
  {
    auto ret = update_geometry(ctrl.mesh[i], geom.meshes[i]);
    if(ret.need_reload)
      return ret;
    any_dirty |= ret.need_upload;
  }

  if(any_dirty)
    geom.dirty_index++;
  return {false, any_dirty};
}

template <typename T>
constexpr auto to_front_face(auto v)
{
  static_constexpr auto m = AVND_ENUM_CONVERTER(
      (ossia::geometry::clockwise, clockwise, cw, CW),
      (ossia::geometry::counter_clockwise, counter_clockwise, ccw, CCW));
  return m(v, decltype(T::front_face){});
}

template <typename T>
constexpr auto to_topology(auto v)
{
  static_constexpr auto m = AVND_ENUM_CONVERTER(
      (ossia::geometry::triangles, triangle, triangles),
      (ossia::geometry::triangle_strip, triangle_strip, triangles_strip),
      (ossia::geometry::triangle_fan, triangle_fan, triangles_fan),
      (ossia::geometry::lines, line, lines), (ossia::geometry::points, point, points));
  return m(v, decltype(T::topology){});
}

template <typename T>
constexpr auto to_cull_mode(auto v)
{
  static_constexpr auto m = AVND_ENUM_CONVERTER(
      (ossia::geometry::front, front, cull_front),
      (ossia::geometry::back, back, cull_back), (ossia::geometry::none, none));
  return m(v, decltype(T::cull_mode){});
}

/// ossia geometry -> avnd geometry input
template <typename T>
  requires(avnd::static_geometry_type<T> || avnd::dynamic_geometry_type<T>)
void mesh_from_ossia(
    const ossia::geometry& src, T& dst, auto&& cpu_to_gpu, auto&& gpu_to_cpu)
{
  if constexpr(requires { dst.vertices; })
    dst.vertices = src.vertices;
  if constexpr(requires { dst.indices; })
    dst.indices = src.indices;
  if constexpr(requires { dst.instances; })
    dst.instances = src.instances;
  dst.topology = to_topology<T>(src.topology);
  dst.front_face = to_front_face<T>(src.front_face);
  dst.cull_mode = to_cull_mode<T>(src.cull_mode);

  {
    dst.buffers.resize(src.buffers.size());
    int buffer_index = 0;
    for(auto& buf : src.buffers)
    {
      using value_type = typename decltype(dst.buffers)::value_type;
      if(auto cpu_src_buf = ossia::get_if<ossia::geometry::cpu_buffer>(&buf.data))
      {
        if constexpr(avnd::geometry_cpu_buffer<value_type>)
        {
          dst.buffers[buffer_index] = value_type{
              .raw_data = cpu_src_buf->raw_data.get(),
              .byte_size = cpu_src_buf->byte_size,
              // .byte_offset = cpu_src_buf->byte_offset, // geometry buffers offsets are with attributes
              .dirty = buf.dirty};
        }
        else if constexpr(avnd::geometry_gpu_buffer<value_type>)
        {
          if(buf.dirty)
          {
            cpu_to_gpu(
                dst.buffers[buffer_index], buffer_index, cpu_src_buf->raw_data.get(),
                cpu_src_buf->byte_size
                // , cpu_src_buf->byte_offset
            );
            dst.buffers[buffer_index].dirty = true;
          }
        }
        else
        {
          static_assert(std::is_void_v<value_type>, "Invalid buffer type");
        }
      }
      else if(auto gpu_src_buf = ossia::get_if<ossia::geometry::gpu_buffer>(&buf.data))
      {
        if constexpr(avnd::geometry_cpu_buffer<value_type>)
        {
          if(buf.dirty)
          {
            gpu_to_cpu(
                dst.buffers[buffer_index], buffer_index, gpu_src_buf->handle
                // , gpu_src_buf->byte_size
                //, gpu_src_buf->byte_offset
            );

            dst.buffers[buffer_index].dirty = true;
          }
        }
        else if constexpr(avnd::geometry_gpu_buffer<value_type>)
        {
          dst.buffers[buffer_index] = value_type{
              .handle = gpu_src_buf->handle,
              .byte_size = gpu_src_buf->byte_size,
              // .byte_offset = gpu_src_buf->byte_offset,
              .dirty = buf.dirty};
        }
        else
        {
          static_assert(std::is_void_v<value_type>, "Invalid buffer type");
        }
      }

      buffer_index++;
    }
  }

  dst.input.clear();
  for(const struct ossia::geometry::input& in : src.input)
  {
    using value_type = typename decltype(dst.input)::value_type;
    dst.input.emplace_back(
        value_type{.buffer = in.buffer, .byte_offset = in.byte_offset});
  }

  if constexpr(requires { dst.index; })
  {
    dst.index.buffer = src.index.buffer;
    dst.index.byte_offset = src.index.byte_offset;
    if(src.index.format == decltype(src.index.format)::uint16)
      dst.index.format = decltype(dst.index.format)::uint16;
    else if(src.index.format == decltype(src.index.format)::uint32)
      dst.index.format = decltype(dst.index.format)::uint32;
  }

  dst.bindings.clear();
  for(const ossia::geometry::binding& in : src.bindings)
  {
    using value_type = typename decltype(dst.bindings)::value_type;
    using stride_type = decltype(value_type::stride);
    using step_rate_type = decltype(value_type::step_rate);
    using classification_type = decltype(value_type::classification);
    dst.bindings.push_back(
        {.stride = static_cast<stride_type>(in.byte_stride),
         .step_rate = static_cast<step_rate_type>(in.step_rate),
         .classification = static_cast<classification_type>(in.classification)});
  }

  dst.attributes.clear();
  for(const ossia::geometry::attribute& in : src.attributes)
  {
    using value_type = typename decltype(dst.attributes)::value_type;
    using binding_type = decltype(value_type::binding);
    using location_type = decltype(value_type::location);
    using format_type = decltype(value_type::format);
    using offset_type = decltype(value_type::byte_offset);
    dst.attributes.push_back(
        {.binding = static_cast<binding_type>(in.binding),
         .location = static_cast<location_type>(in.location),
         .format = static_cast<format_type>(in.format),
         .byte_offset = static_cast<offset_type>(in.byte_offset)});
  }
}

template <typename T>
  requires(avnd::static_geometry_type<T> || avnd::dynamic_geometry_type<T>)
void meshes_from_ossia(
    const ossia::mesh_list_ptr& src, T& dst, auto&& cpu_to_gpu, auto&& gpu_to_cpu)
{
  if(!src)
    return;

  if(src->meshes.empty())
  {
    dst = T{};
    return;
  }

  // For now do only one
  mesh_from_ossia(src->meshes[0], dst, cpu_to_gpu, gpu_to_cpu);
}

/*
template <typename T>
  requires(
      avnd::static_geometry_type<typename decltype(T::mesh)::value_type>
      || avnd::dynamic_geometry_type<typename decltype(T::mesh)::value_type>)
void meshes_from_ossia(const ossia::mesh_list_ptr& src, T& dst)
{
  meshes_from_ossia(src, dst.mesh);
}
  */
template <typename T>
void geometry_from_ossia(
    const ossia::geometry_port& src, T& dst, auto&& cpu_to_gpu, auto&& gpu_to_cpu)
    = delete;
/*
{
  meshes_from_ossia(src.geometry.meshes, dst.mesh, cpu_to_gpu, gpu_to_cpu);
  static_assert(sizeof(dst.transform) / sizeof(dst.transform[0]) == 16);
  std::copy_n(src.transform.matrix, std::ssize(src.transform.matrix), dst.transform);
}*/
}
