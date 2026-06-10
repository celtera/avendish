#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/helpers.hpp>
#include <avnd/binding/max/jitter_helpers.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/concepts/tensor.hpp>
#include <avnd/introspection/input.hpp>
#include <halp/tensor_port.hpp>
#include <boost/mp11.hpp>
#include <array>
#include <jit.common.h>
#include <max.jit.mop.h>
namespace max
{

// GPU texture ports (halp::gpu_texture_input & co) have no CPU-side pixel data
// that a jit matrix could be read into: they are ignored by the Max binding
// for now instead of being treated as MOP matrix inputs.
template <typename Field>
concept max_jit_input
    = (avnd::matrix_port<Field> || avnd::tensor_port<Field>)
      && !avnd::gpu_texture_port<Field>;

template <typename Field>
using is_max_jit_input_t = boost::mp11::mp_bool<max_jit_input<Field>>;

template <typename T>
using max_jit_input_introspection
    = avnd::predicate_introspection<typename avnd::inputs_type<T>::type, is_max_jit_input_t>;

template <typename Field>
using is_explicit_parameter_t
    = boost::mp11::mp_bool<avnd::parameter_port<Field> && !avnd::attribute_port<Field>>;
template<typename T>
using explicit_parameter_input_introspection = avnd::predicate_introspection<typename avnd::inputs_type<T>::type, is_explicit_parameter_t>;

template <typename T>
struct inputs
{
  void init(avnd::effect_container<T>& implementation, t_object& x_obj) { }

  void for_inlet(int inlet, avnd::effect_container<T>& implementation, auto&& func)
  {
  }
};

template <typename T>
  requires(explicit_parameter_input_introspection<T>::size > 0
           && max_jit_input_introspection<T>::size == 0)
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
      for(int i = 1; i < refl::size; i++) {
        proxies[i-1] = proxy_new(&x_obj, 1024 + refl::size - i - 1, 0L);
      }
    }
    else
    {
      for(int i = 0; i < refl::size; i++)
        proxies[i] = proxy_new(&x_obj, 1024 + refl::size - i - 1, 0L);
    }
  }

  void for_inlet(int inlet, avnd::effect_container<T>& implementation, auto&& func)
  {
    if constexpr(first_parameter_is_explicit)
    {
      // All inlets are explicit parameters
      if(inlet == 0)
      {
        explicit_parameter_input_introspection<T>::for_nth_mapped(
            avnd::get_inputs<T>(implementation), 0, [&func](auto& field) {
          func(field);
        });
      }
      else
      {
        explicit_parameter_input_introspection<T>::for_nth_mapped(
            avnd::get_inputs<T>(implementation), inlet - 1024 + 1, [&func](auto& field) {
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
            avnd::get_inputs<T>(implementation), 0, [&func](auto& field) {
          func(field);
        });
      }
      else
      {
        explicit_parameter_input_introspection<T>::for_nth_mapped(
            avnd::get_inputs<T>(implementation), inlet - 1024, [&func](auto& field) {
          func(field);
        });
      }
    }
  }
};

template <typename T>
  requires(avnd::parameter_input_introspection<T>::size > 0
           && explicit_parameter_input_introspection<T>::size == 0
           && max_jit_input_introspection<T>::size == 0)
struct inputs<T>
{
  void init(avnd::effect_container<T>& implementation, t_object& x_obj) { }

  void for_inlet(int inlet, avnd::effect_container<T>& implementation, auto&& func)
  {
    // Inlet is necessarily 0
    avnd::parameter_input_introspection<T>::for_nth_mapped(
        avnd::get_inputs<T>(implementation), 0, [&func](auto& field) {
      func(field);
    });
  }
};


template <typename T>
  requires(max_jit_input_introspection<T>::size > 0)
struct inputs<T>
{
  template <typename Field>
  static void configure_mop_input(void* input)
  {
    if(!input)
      return;

    if constexpr(avnd::tensor_port<Field>)
    {
      using value_type = std::remove_cvref_t<decltype(Field::value)>;
      using element_type = avnd::tensor_element<value_type>;
      t_symbol* type = jitter::jitter_type_for_element<element_type>();

      constexpr std::size_t static_rank = halp::static_port_rank<Field>();
      long mindim = 1;
      long maxdim = jitter::jitter_max_dimcount;
      if constexpr(static_rank != avnd::dynamic_rank)
      {
        mindim = static_cast<long>(static_rank);
        maxdim = static_cast<long>(static_rank);
      }

      jit_attr_setsym(input, _jit_sym_type, type);
      jit_attr_setlong(input, _jit_sym_planecount, 1);
      jit_attr_setlong(input, _jit_sym_mindimcount, mindim);
      jit_attr_setlong(input, _jit_sym_maxdimcount, maxdim);
    }
    else
    {
      jit_attr_setsym(input, _jit_sym_type, _jit_sym_char);
      jit_attr_setlong(input, _jit_sym_planecount, 4); // RGBA
      jit_attr_setlong(input, _jit_sym_mindimcount, 2); // 2D minimum
      jit_attr_setlong(input, _jit_sym_maxdimcount, 2); // 2D maximum
    }
  }

  void init(avnd::effect_container<T>& implementation, t_object& x_obj)
  {
    // Setup MOP inputs
    const auto x = &x_obj;
    static constexpr auto matrix_input_count = max_jit_input_introspection<T>::size;
    {
      for (int i = matrix_input_count; i > 1; i--) {
        max_jit_obex_proxy_new(x, i - 1);
      }

      // Add variable inputs if needed
      if (matrix_input_count > 0) {
        max_jit_mop_variable_addinputs(x, matrix_input_count);
      }

      int slot = 0;
      max_jit_input_introspection<T>::for_all(
          avnd::get_inputs<T>(implementation),
          [&]<typename Field>(Field&) {
            if(void* input = max_jit_mop_getinput(x, slot))
              configure_mop_input<Field>(input);
            ++slot;
          });
    }

    const auto mop = max_jit_obex_adornment_get(x, _jit_sym_jit_mop);
    const auto incount = jit_attr_getlong(mop, _jit_sym_inputcount);

    // add proxy inlet and internal matrix for
    // all inputs except leftmost inlet
    for (int i = 2; i <= incount; i++)
    {
      max_jit_obex_proxy_new(x, (incount + 1) - i); // right to left
      if (auto p = jit_object_method(mop, _jit_sym_getinput, i))
      {
        t_jit_matrix_info info;
        jit_matrix_info_default(&info);
        max_jit_mop_restrict_info(x, p, &info);

        auto name = jit_symbol_unique();
        auto m = jit_object_new(_jit_sym_jit_matrix, &info);
        m = jit_object_register(m,name);

        jit_attr_setsym(p, _jit_sym_matrixname, name);
        jit_object_method(p, _jit_sym_matrix, m);
        jit_object_attach(name, x);
      }
    }
  }

  void for_inlet(int inlet, avnd::effect_container<T>& implementation, auto&& func)
  {
    // All parameters are on 0
    // FIXME make sure this does not get called for non-parameter inlets?
    avnd::parameter_input_introspection<T>::for_nth_mapped(
        avnd::get_inputs<T>(implementation), 0, [&func](auto& field) {
      func(field);
    });
  }
};

}
