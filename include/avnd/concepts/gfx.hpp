#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/aggregates.hpp>
#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{
template <typename T>
concept cpu_raw_buffer = requires(T t) {
  t.bytes;
  t.bytesize;
  t.changed;
  typename T::format;
};

template <typename T>
concept cpu_buffer_port = requires(T t) {
  t.buffer;
} && (cpu_raw_buffer<std::decay_t<decltype(std::declval<T>().buffer)>>);

template <typename T>
concept cpu_fixed_format_texture = requires(T t) {
  t.bytes;
  t.width;
  t.height;
  t.changed;
  typename T::format;
};
template <typename T>
concept cpu_dynamic_format_texture = requires(T t) {
  t.bytes;
  t.width;
  t.height;
  t.changed;
  t.format = {};
};
template <typename T>
concept cpu_texture
    = cpu_raw_buffer<T> || cpu_fixed_format_texture<T> || cpu_dynamic_format_texture<T>;

template <typename T>
concept cpu_texture_port = requires(T t) {
  t.texture;
} && (cpu_texture<std::decay_t<decltype(std::declval<T>().texture)>>);

template <typename T>
concept sampler_port = requires(T t) { T::sampler(); };

template <typename T>
concept image_port = requires(T t) { T::image(); };

template <typename T>
concept attachment_port = requires { T::attachment(); };

template <typename T>
concept texture_port
    = cpu_texture_port<T> || sampler_port<T> || attachment_port<T> || image_port<T>;

template <typename T>
concept uniform_port = requires { T::uniform(); };

template <typename T>
concept static_geometry_type = requires(T t)
{
  sizeof(t.buffers);
  sizeof(t.input);
  sizeof(typename T::attributes);
  sizeof(typename T::bindings);
}
&&(
    requires(T t) { t.vertices; } || requires(T t) { t.indices; });
template <typename T>
concept dynamic_geometry_type = requires(T t)
{
  t.buffers.size();
  t.input.size();
  t.attributes.size();
  t.bindings.size();
}
&&(
    requires(T t) { t.vertices; } || requires(T t) { t.indices; });
template <typename T>
concept geometry_port = static_geometry_type<T> || dynamic_geometry_type<T>
                        || static_geometry_type<decltype(T::mesh)>
                        || dynamic_geometry_type<decltype(T::mesh)>
                        || static_geometry_type<typename decltype(T::mesh)::value_type>
                        || dynamic_geometry_type<typename decltype(T::mesh)::value_type>;
}

/*
struct tester {
    struct my_cpu_tex {
      unsigned char* bytes;
      int width;
      int height;
      int format;
    };
    struct my_gpu_tex {
      std::intptr_t handle;
      int width;
      int height;
      int format;
    };
    static_assert(avnd::cpu_texture<my_cpu_tex>);
    static_assert(avnd::gpu_texture<my_gpu_tex>);

    struct A {
      my_cpu_tex texture;
    };
    struct B {
      my_gpu_tex texture;
    };
    static_assert(avnd::cpu_texture_port<A>);
    static_assert(avnd::gpu_texture_port<B>);

};
*/

namespace avnd
{
type_or_value_qualification(geometry)
type_or_value_reflection(geometry)
type_or_value_accessors(geometry)

type_or_value_qualification(buffers)
type_or_value_reflection(buffers)
type_or_value_accessors(buffers)

type_or_value_qualification(bindings)
type_or_value_reflection(bindings)
type_or_value_accessors(bindings)

type_or_value_qualification(attributes)
type_or_value_reflection(attributes)
type_or_value_accessors(attributes)

type_or_value_qualification(input)
type_or_value_reflection(input)
type_or_value_accessors(input)
}
