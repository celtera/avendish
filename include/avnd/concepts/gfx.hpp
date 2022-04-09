#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{

template <typename T>
concept cpu_texture = requires(T t)
{
  t.bytes;
  t.width;
  t.height;
  t.changed;
  typename T::format;
};

template <typename T>
concept cpu_texture_port = requires(T t)
{
  t.texture;
}
&&cpu_texture<std::decay_t<decltype(std::declval<T>().texture)>>;

template <typename T>
concept sampler_port = requires(T t)
{ T::sampler(); };

template <typename T>
concept image_port = requires(T t)
{ T::image(); };

template <typename T>
concept attachment_port = requires {
  T::attachment();
};

template <typename T>
concept texture_port =
   cpu_texture_port<T>
|| sampler_port<T>
|| attachment_port<T>
|| image_port<T>
;


template <typename T>
concept uniform_port = requires {
  T::uniform();
};
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
