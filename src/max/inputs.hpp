#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts.hpp>
#include <avnd/input_introspection.hpp>
#include <max/helpers.hpp>
#include <array>
namespace max
{

template <typename T>
struct inputs
{
  using refl = avnd::parameter_input_introspection<T>;
  // TODO free them
  static constexpr int proxy_count = refl::size - 1;

  std::array<void*, proxy_count> proxies;
  void init(T& implementation, t_object& x_obj)
  {
    int n = proxy_count;
    int k = 0;
    refl::for_all(
        implementation.inputs,
        [&x_obj, &n, &k, this](auto& ctl)
        {
        if(n-- > 0)
          proxies[k++] = proxy_new(&x_obj, n + 1, 0L);
        });
  }
};

}
