#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/helpers.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/input.hpp>
#include <boost/mp11.hpp>
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

  void for_inlet(int inlet, auto& inputs, auto&& func)
  {
  }
};

template <typename T>
  requires(explicit_parameter_input_introspection<T>::size > 0)
struct inputs<T>
{
  using refl = explicit_parameter_input_introspection<T>;
  static constexpr bool first_parameter_is_explicit = !avnd::attribute_port<boost::mp11::mp_front<typename avnd::parameter_input_introspection<T>::fields>>;
  static constexpr int proxy_count = first_parameter_is_explicit ? refl::size - 1 : refl::size;

  // TODO free them
  std::array<void*, proxy_count> proxies;
  void init(avnd::effect_container<T>& implementation, t_object& x_obj)
  {
    // Example 1:
    // - params a:explicit, b:explicit, c:explicit
    // - then inlet 0: a (index: 0) -> N/A
    //        proxy 1: b (index: 1) -> proxy 1
    //        proxy 2: c (index: 2) -> proxy 2
    // Example 2:
    // - params a:attribute, b:explicit, c:explicit
    // - then inlet 0: a
    //        proxy 1: b (index: 0) -> proxy 0
    //        proxy 2: c (index: 1) -> proxy 1
    // Example 3:
    // - params a:attribute, b:attribute, c:explicit
    // - then inlet 0: a, b
    //        proxy 1: c (index: 0) -> proxy 0

    if constexpr(first_parameter_is_explicit)
    {
      for(int i = 1; i < refl::size; i++)
        proxies[i] = proxy_new(&x_obj, 1024 + refl::size - i - 1, 0L);
    }
    else
    {
      for(int i = 0; i < refl::size; i++)
        proxies[i] = proxy_new(&x_obj, 1024 + refl::size - i - 1, 0L);
    }
  }

  void for_inlet(int inlet, auto& self, auto&& func)
  {
    if constexpr(first_parameter_is_explicit)
    {
      // All inlets are explicit parameters
      if(inlet == 0)
      {
        explicit_parameter_input_introspection<T>::for_nth_mapped(
            avnd::get_inputs<T>(self.implementation), 0, [&func](auto& field) {
          func(field);
        });
      }
      else
      {
        explicit_parameter_input_introspection<T>::for_nth_mapped(
            avnd::get_inputs<T>(self.implementation), inlet - 1024 + 1, [&func](auto& field) {
          func(field);
        });
      }
    }
    else
    {
      // First inlet is only for attributes
      if(inlet == 0)
      {
        // Set the first parameter
        avnd::parameter_input_introspection<T>::for_nth_mapped(
            avnd::get_inputs<T>(self.implementation), 0, [&func](auto& field) {
          func(field);
        });
      }
      else
      {
        explicit_parameter_input_introspection<T>::for_nth_mapped(
            avnd::get_inputs<T>(self.implementation), inlet - 1024, [&func](auto& field) {
          func(field);
        });
      }
    }
  }
};

template <typename T>
  requires(avnd::parameter_input_introspection<T>::size > 0 && explicit_parameter_input_introspection<T>::size == 0)
struct inputs<T>
{
  void init(avnd::effect_container<T>& implementation, t_object& x_obj) { }

  void for_inlet(int inlet, auto& self, auto&& func)
  {
    // Inlet is necessarily 0
    avnd::parameter_input_introspection<T>::for_nth_mapped(
        avnd::get_inputs<T>(self.implementation), 0, [&func](auto& field) {
      func(field);
    });
  }
};

}
