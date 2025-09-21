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

template <typename T>
  requires(avnd::static_geometry_type<T> || avnd::dynamic_geometry_type<T>)
void load_geometry(T& ctrl, ossia::geometry& geom)
{
  if constexpr(requires { ctrl.vertices; })
    geom.vertices = ctrl.vertices;
  if constexpr(requires { ctrl.indices; })
    geom.indices = ctrl.indices;

  geom.topology = get_topology(ctrl);
  geom.front_face = get_front_face(ctrl);
  geom.cull_mode = get_cull_mode(ctrl);

  {
    std::size_t buf_k = 0;
    avnd::for_each_field_ref(ctrl.buffers, [&](auto& buf) {
      using ptr_type = std::decay_t<decltype(buf.data)>;
      if constexpr(std::is_same_v<ptr_type, void*>)
      {
        // using data_type = float[4];
        // Dynamic case
        if(buf_k >= geom.buffers.size())
        {
          ossia::geometry::cpu_buffer cpu_buf;
          auto data = std::make_unique<char[]>(buf.size);
          std::copy_n((char*)buf.data, buf.size, data.get());
          cpu_buf.data = std::move(data);
          cpu_buf.size = buf.size;
          ossia::geometry::buffer in{.data = std::move(cpu_buf), .dirty = true};
          buf.dirty = false;
          in.dirty = true;
          geom.buffers.push_back(std::move(in));
        }
        else if(buf.dirty)
        {
          ossia::geometry::cpu_buffer cpu_buf;
          auto data = std::make_unique<char[]>(buf.size);
          std::copy_n((char*)buf.data, buf.size, data.get());
          cpu_buf.data = std::move(data);
          cpu_buf.size = buf.size;

          // FIXME recycle old in.data
          auto& in = geom.buffers[buf_k];
          in.data = std::move(cpu_buf);
          buf.dirty = false;
          in.dirty = true;
        }
      }
      else
      {
        // Static case
        using data_type = std::decay_t<decltype(buf.data[0])>;
        static_assert(!std::is_reference_v<data_type>);
        static_assert(!std::is_const_v<data_type>);

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
            auto data = std::make_unique<data_type[]>(buf.size);
            std::copy_n(buf.data, buf.size, data.get());
            cpu_buf.data = std::move(data);
            cpu_buf.size = buf.size * sizeof(data_type);
          }

          ossia::geometry::buffer in{.data = std::move(cpu_buf), .dirty = true};
          buf.dirty = false;

          geom.buffers.push_back(std::move(in));
        }
        else if(buf.dirty)
        {
          ossia::geometry::cpu_buffer cpu_buf;
          auto data = std::make_unique<data_type[]>(buf.size);
          std::copy_n(buf.data, buf.size, data.get());
          cpu_buf.data = std::move(data);
          cpu_buf.size = buf.size * sizeof(data_type);

          // FIXME recycle old in.data
          auto& in = geom.buffers[buf_k];
          in.data = std::move(cpu_buf);
          buf.dirty = false;
          in.dirty = true;
        }
      }
      buf_k++;
    });
  }

  geom.input.clear();
  avnd::for_each_field_ref(ctrl.input, [&](auto& input) {
    struct ossia::geometry::input in;

    if constexpr(requires { input.buffer(); })
      in.buffer = avnd::index_in_struct(ctrl.buffers, input.buffer());
    else
      in.buffer = input.buffer;

    in.offset = input.offset;

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

    if constexpr(requires { ctrl.index.offset(); })
      geom.index.offset = ctrl.index.offset();
    else if constexpr(requires { ctrl.index.offset; })
      geom.index.offset = ctrl.index.offset;
    else
      geom.index.offset = 0;
  }

  if(geom.bindings.empty())
  {
    avnd::for_each_field_ref(avnd::get_bindings(ctrl), [&](auto& binding) {
      ossia::geometry::binding b;

      b.stride = binding.stride;
      if constexpr(requires { binding.classification; })
      {
        static_constexpr auto m = AVND_ENUM_OR_TAG_MATCHER(
            classification, (ossia::geometry::binding::per_vertex, per_vertex),
            (ossia::geometry::binding::per_instance, per_instance));
        b.classification
            = m(binding.classification, ossia::geometry::binding::per_vertex);
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

      if constexpr(requires { attr.offset; })
        a.offset = attr.offset;

      if constexpr(requires { typename A::datatype; })
      {
        using tp = typename A::datatype;
        if constexpr(std::is_same_v<tp, float>)
          a.format = ossia::geometry::attribute::fp1;
        else if constexpr(std::is_same_v<tp, float[2]>)
          a.format = ossia::geometry::attribute::fp2;
        else if constexpr(std::is_same_v<tp, float[3]>)
          a.format = ossia::geometry::attribute::fp3;
        else if constexpr(std::is_same_v<tp, float[4]>)
          a.format = ossia::geometry::attribute::fp4;
        else if constexpr(
            std::is_same_v<tp, uint8_t> || std::is_same_v<tp, unsigned char>)
          a.format = ossia::geometry::attribute::unsigned1;
        else if constexpr(std::is_same_v<tp, uint16_t>)
          a.format = ossia::geometry::attribute::unsigned2;
        else if constexpr(std::is_same_v<tp, uint32_t>)
          a.format = ossia::geometry::attribute::unsigned4;
      }
      else
      {
        static_constexpr auto m = AVND_ENUM_MATCHER(
            (ossia::geometry::attribute::fp1, float1, fp1, Float1),
            (ossia::geometry::attribute::fp2, float2, fp2, Float2, vec2),
            (ossia::geometry::attribute::fp3, float3, fp3, Float3, vec3),
            (ossia::geometry::attribute::fp4, float4, fp4, Float4, vec4),
            (ossia::geometry::attribute::unsigned1, unsigned1, u1, uchar1),
            (ossia::geometry::attribute::unsigned2, unsigned2, u2, uchar2, uvec2),
            (ossia::geometry::attribute::unsigned4, unsigned4, u4, uchar4, uvec4));
        a.format = m(attr.format, ossia::geometry::attribute::fp4);
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

template <typename T>
  requires(avnd::static_geometry_type<T> || avnd::dynamic_geometry_type<T>)
void mesh_from_ossia(const ossia::geometry& src, T& dst)
{
  if constexpr(requires { dst.vertices; })
    dst.vertices = src.vertices;
  if constexpr(requires { dst.indices; })
    dst.indices = src.indices;
  dst.topology = to_topology<T>(src.topology);
  dst.front_face = to_front_face<T>(src.front_face);
  dst.cull_mode = to_cull_mode<T>(src.cull_mode);

  {
    dst.buffers.clear();
    for(auto& buf : src.buffers)
    {
      using value_type = typename decltype(dst.buffers)::value_type;
      if(auto cpu_buf = ossia::get_if<ossia::geometry::cpu_buffer>(&buf.data))
      {
        dst.buffers.emplace_back(
            value_type{
                .data = cpu_buf->data.get(), .size = cpu_buf->size, .dirty = buf.dirty});
      }
      else
      {
        // FIXME need to download gpu -> cpu
      }
    }
  }

  dst.input.clear();
  for(const struct ossia::geometry::input& in : src.input)
  {
    using value_type = typename decltype(dst.input)::value_type;
    dst.input.emplace_back(value_type{.buffer = in.buffer, .offset = in.offset});
  }

  if constexpr(requires { dst.index; })
  {
    dst.index.buffer = src.index.buffer;
    dst.index.offset = src.index.offset;
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
    dst.bindings.push_back({
        .stride = static_cast<stride_type>(in.stride),
        .step_rate = static_cast<step_rate_type>(in.step_rate),
        .classification = static_cast<classification_type>(in.classification)
    });
  }

  dst.attributes.clear();
  for(const ossia::geometry::attribute& in : src.attributes)
  {
    using value_type = typename decltype(dst.attributes)::value_type;
    using binding_type = decltype(value_type::binding);
    using location_type = decltype(value_type::location);
    using format_type = decltype(value_type::format);
    using offset_type = decltype(value_type::offset);
    dst.attributes.push_back(
        {.binding = static_cast<binding_type>(in.binding),
         .location = static_cast<location_type>(in.location),
         .format = static_cast<format_type>(in.format),
         .offset = static_cast<offset_type>(in.offset)});
  }
}

template <typename T>
  requires(avnd::static_geometry_type<T> || avnd::dynamic_geometry_type<T>)
void meshes_from_ossia(const ossia::mesh_list_ptr& src, T& dst)
{
  if(!src)
    return;

  if(src->meshes.empty())
  {
    dst = T{};
    return;
  }

  // For now do only one
  mesh_from_ossia(src->meshes[0], dst);
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
void geometry_from_ossia(const ossia::geometry_port& src, T& dst)
{
  meshes_from_ossia(src.geometry.meshes, dst.mesh);
  static_assert(sizeof(dst.transform) / sizeof(dst.transform[0]) == 16);
  std::copy_n(src.transform.matrix, std::ssize(src.transform.matrix), dst.transform);
}
}
