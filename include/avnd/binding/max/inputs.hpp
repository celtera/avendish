#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/helpers.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/input.hpp>

#include <array>
namespace max
{

template <typename Field>
using is_explicit_parameter_t = boost::mp11::mp_bool<avnd::parameter<Field> && !avnd::attribute_port<Field>>;
template<typename T>
using explicit_parameter_input_introspection = avnd::predicate_introspection<typename avnd::inputs_type<T>::type, is_explicit_parameter_t>;

template <typename T>
struct inputs
{
  void init(avnd::effect_container<T>& implementation, t_object& x_obj) { }
};

template <typename T>
  requires(explicit_parameter_input_introspection<T>::size > 0)
struct inputs<T>
{
  using refl = explicit_parameter_input_introspection<T>;
  // TODO free them
  static constexpr int proxy_count = refl::size - 1;

  std::array<void*, proxy_count> proxies;
  void init(avnd::effect_container<T>& implementation, t_object& x_obj)
  {
    int n = proxy_count;
    int k = 0;
    refl::for_all(
        avnd::get_inputs<T>(implementation), [&x_obj, &n, &k, this](auto& ctl) {
          if(n-- > 0)
            proxies[k++] = proxy_new(&x_obj, n + 1, 0L);
        });
  }
};

}
