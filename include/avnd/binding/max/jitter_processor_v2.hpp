#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/jitter_helpers.hpp>
#include <avnd/binding/max/processor_common.hpp>
#include <avnd/binding/max/attributes_setup.hpp>
#include <avnd/binding/max/helpers.hpp>
#include <avnd/binding/max/init.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <fmt/format.h>
#include <ext.h>
#include <ext_obex.h>
#include <jit.common.h>
#include <max.jit.mop.h>
#include <vector>

namespace max
{
template <typename T>
struct avnd_jit_class
{
  t_jit_object ob;

  avnd::effect_container<T> processor;

  std::vector<void*> input_matrices;
  std::vector<void*> output_matrices;

  static constexpr int texture_input_count = avnd::cpu_texture_input_introspection<T>::size ;
  static constexpr int texture_output_count = avnd::cpu_texture_output_introspection<T>::size;

  void init()
  {
    avnd::init_controls(processor);
    avnd::prepare(processor, {});

    input_matrices.resize(texture_input_count);
    output_matrices.resize(texture_output_count);
  }

  static t_jit_err matrix_calc_imp(avnd_jit_class<T>* x, void* inputs, void* outputs)
  {
    if(!x || !inputs || !outputs)
      return JIT_ERR_INVALID_PTR;

    // Process texture inputs
    if constexpr(texture_input_count > 0)
    {
      int idx = 0;
      avnd::cpu_texture_input_introspection<T>::for_all(
        avnd::get_inputs(x->processor),
        [&](auto& field) {
          if(idx < texture_input_count)
          {
            // Get input matrix
            void* in_matrix = jit_object_method(inputs, _jit_sym_getindex, idx);
            if(in_matrix)
            {
              // Convert matrix to texture
              ::max::jitter::matrix_to_texture(in_matrix, field);
            }
            idx++;
          }
        });
    }

    // Execute the processor
    auto& proc = x->processor.effect;
    if constexpr(requires { proc(); })
    {
      proc();
    }

    // Process texture outputs
    if constexpr(texture_output_count > 0)
    {
      int idx = 0;
      avnd::cpu_texture_output_introspection<T>::for_all(
        avnd::get_outputs(x->processor),
        [&](auto& field) {
          if(void* out_matrix = jit_object_method(outputs, _jit_sym_getindex, idx))
          {
            if(const auto& tex = field.texture; tex.bytes && tex.width > 0 && tex.height > 0)
            {
              // Convert texture to matrix
              jitter::texture_to_matrix(field, out_matrix);
            }
          }
          idx++;
        });
    }

    return JIT_ERR_NONE;
  }

  template<typename Field>
  static t_symbol* get_jitter_type_for_parameter()
  {
    using value_type = std::decay_t<decltype(Field::value)>;

    if constexpr(std::is_same_v<char, value_type>)
      return _jit_sym_char;
    else if constexpr(std::is_same_v<unsigned char, value_type>)
      return _jit_sym_char;
    else if constexpr(std::is_same_v<signed char, value_type>)
      return _jit_sym_char;
    else if constexpr(std::is_integral_v<value_type>)
      return _jit_sym_long;
    else if constexpr(std::is_same_v<float, value_type>)
      return _jit_sym_float32;
    else if constexpr(std::is_same_v<double, value_type>)
      return _jit_sym_float64;
    else if constexpr(avnd::string_ish<value_type>)
      return _jit_sym_symbol;
    else
      return _jit_sym_atom;
  }
};

template <typename T>
struct max_jit_wrapper : processor_common<T>
{
  t_object ob;
  void* obex{};

  void process()
  {
    void* mop = max_jit_obex_adornment_get(this, _jit_sym_jit_mop);

    if(!mop)
      return;

    // Since we have MAX_JIT_MOP_FLAGS_OWN_BANG, we need to call mproc ourselves
    process_mop(mop);

    // Then output the matrix
    max_jit_mop_outputmatrix(this);
  }

   void process(t_atom_long n)
  {
    // FIXME
    process();
  }

   void process( t_atom_float f)
  {
    // FIXME
    process();
  }

   void process( t_symbol* s)
  {
    // FIXME
    process();
  }

  void process_dict(t_symbol* name)
  {
    // FIXME
    process();
  }

  void process_mop(void* mop)
  {
    void* jit_ob = max_jit_obex_jitob_get(this);
    if(!jit_ob)
      return;

    // Get input and output lists
    void* inputlist = jit_object_method(mop, _jit_sym_getinputlist);
    void* outputlist = jit_object_method(mop, _jit_sym_getoutputlist);

    // Process via matrix_calc
    t_jit_err err = (t_jit_err)jit_object_method(
        jit_ob,
        _jit_sym_matrix_calc,
        inputlist,
        outputlist);

    if(err)
      jit_error_code(jit_ob, err);
  }

   void process(t_symbol* s, long argc, t_atom* argv)
  {
    // FIXME
    process();
  }
};


template<typename T>
static inline T* jit_new(t_class* cls)
{
  if(auto obj = jit_object_alloc(cls))
  {
    t_jit_object tmp;
    memcpy(&tmp, obj, sizeof(t_jit_object));
    auto x = new(obj) T{};
    memcpy((void*)x, &tmp, sizeof(t_jit_object));
    x->init();
    return x;
  }
  return nullptr;
}


template <typename T>
struct jitter_processor_metaclass
{
  static inline t_class* g_jit_class = nullptr;
  static inline t_class* g_class = nullptr;

  static inline jitter_processor_metaclass* instance{};

  using attrs = avnd::attribute_input_introspection<T>;
  static constexpr int num_attributes = attrs::size;
  static inline std::array<t_object*, num_attributes> attributes;

  static constexpr int texture_input_count = avnd_jit_class<T>::texture_input_count;
  static constexpr int texture_output_count = avnd_jit_class<T>::texture_output_count;

  jitter_processor_metaclass()
  {
    instance = this;

    this->jit_class_init();
    this->max_class_init();
  }

  static t_jit_err jit_class_init()
  {
    if(g_jit_class)
      return JIT_ERR_NONE; // Already registered

    void* mop = nullptr;
    void* attr = nullptr;
    long attrflags = 0;

    // Create Jitter class with unique name
    const auto jit_class_name = fmt::format("jit_avnd_{}", avnd::get_c_name<T>());

    g_jit_class = (t_class*)jit_class_new(
        jit_class_name.c_str(),
        (method)+[] () { return jit_new<avnd_jit_class<T>>(g_jit_class); },
        (method)+[] (avnd_jit_class<T>* x) { std::destroy_at(x); },
        sizeof(avnd_jit_class<T>),
        0L);

    if(!g_jit_class)
      return JIT_ERR_OUT_OF_MEM;

    // Create MOP (Matrix Operator) with appropriate inputs/outputs
    // For generators (no inputs), we need at least 1 input to receive bangs
    const int mop_input_count = (texture_input_count == 0) ? 1 : texture_input_count;
    mop = jit_object_new(_jit_sym_jit_mop, mop_input_count, texture_output_count);

    if(mop)
    {
      // Configure MOP inputs
      if(texture_input_count == 0)
      {
        // For generators, we have a dummy input for bangs
        // Don't configure it as a matrix input
      }
      else
      {
        for(int i = 0; i < texture_input_count; i++)
        {
          void* input = jit_object_method(mop, _jit_sym_getinput, i + 1);
          if(input)
          {
            jit_attr_setsym(input, _jit_sym_type, _jit_sym_char);
            jit_attr_setlong(input, _jit_sym_planecount, 4); // RGBA
            jit_attr_setlong(input, _jit_sym_mindimcount, 2); // 2D minimum
            jit_attr_setlong(input, _jit_sym_maxdimcount, 2); // 2D maximum
          }
        }
      }

      // Configure MOP outputs
      for(int i = 0; i < texture_output_count; i++)
      {
        void* output = jit_object_method(mop, _jit_sym_getoutput, i + 1);
        if(output)
        {
          jit_attr_setsym(output, _jit_sym_type, _jit_sym_char);
          jit_attr_setlong(output, _jit_sym_planecount, 4); // RGBA
          jit_attr_setlong(output, _jit_sym_mindimcount, 2); // 2D
          jit_attr_setlong(output, _jit_sym_maxdimcount, 2); // 2D
        }
      }

      // Add MOP to class
      jit_class_addadornment(g_jit_class, (t_jit_object*)mop);
    }

    // Add matrix_calc method (required for MOP)
    jit_class_addmethod(g_jit_class, (method)avnd_jit_class<T>::matrix_calc_imp, "matrix_calc", A_CANT, 0L);

    jit_class_register(g_jit_class);

    return JIT_ERR_NONE;
  }

  static void max_class_init()
  {
    using instance = max_jit_wrapper<T>;
    t_class *jclass{};

    static const std::string max_class_name = std::string(avnd::get_c_name<T>().data());

    static constexpr auto obj_new = +[](t_symbol* s, long argc, t_atom* argv)
    {
      static const std::string jit_class_name = "jit_avnd_" + std::string(avnd::get_c_name<T>().data());

      auto sym_class_name = gensym(jit_class_name.c_str());
      auto* x = (max_jit_wrapper<T>*)max_jit_object_alloc(g_class, sym_class_name);

      if(x)
      {
        if(void* jit_ob = jit_object_new(sym_class_name))
        {
          max_jit_mop_setup_simple(x, jit_ob, argc, argv);
          max_jit_attr_args(x, argc, argv);
        }
        else
        {
          object_free((t_object*)x);
          x = nullptr;
        }
      }
      return x;
    };

    static constexpr auto obj_free = +[](max_jit_wrapper<T>* x) {
      if(x) {
        std::destroy_at(x);
        max_jit_mop_free(x);
        max_jit_object_free(x);
      }
    };

    g_class = class_new(
        max_class_name.c_str(),
        (method)obj_new,
        (method)obj_free,
        sizeof(max_jit_wrapper<T>),
        (method)nullptr,
        A_GIMME,
        0);

    max_jit_class_obex_setup(g_class, calcoffset(max_jit_wrapper<T>, obex));

    max_jit_class_mop_wrap(g_class, g_jit_class, 0);

    // Skip standard wrap to avoid attribute recursion issues
    // Since we don't have custom attributes yet, this avoids the infinite loop
    // max_jit_class_wrap_standard(g_class, g_jit_class, 0);

    static constexpr auto obj_process
        = +[](instance* obj, t_symbol* s, int argc, t_atom* argv) -> void {
      obj->process(s, argc, argv);
    };

    static constexpr auto obj_process_bang = +[](instance* obj) -> void { obj->process(); };

    static constexpr auto obj_process_int
        = +[](instance* obj, t_atom_long value) -> void { obj->process(value); };

    static constexpr auto obj_process_float
        = +[](instance* obj, t_atom_float value) -> void { obj->process(value); };

    static constexpr auto obj_process_sym
        = +[](instance* obj, t_symbol* value) -> void { obj->process(value); };

    static constexpr auto obj_process_dict
        = +[](instance* obj, t_symbol* value) -> void { obj->process_dict(value); };

  static constexpr auto obj_assist = processor_common<T>::obj_assist;

    static constexpr auto obj_process_mop
        = +[](instance* obj, void* mop) -> void { obj->process_mop(mop); };

    class_addmethod(g_class, (method)obj_process_mop, "mproc", A_CANT, 0);
    class_addmethod(g_class, (method)obj_process_int, "int", A_LONG, 0);
    class_addmethod(g_class, (method)obj_process_float, "float", A_FLOAT, 0);
    class_addmethod(g_class, (method)obj_process_sym, "symbol", A_SYM, 0);
    class_addmethod(g_class, (method)obj_process_bang, "bang", A_NOTHING, 0);
    class_addmethod(g_class, (method)obj_process_dict, "dict", A_SYM, 0);
    class_addmethod(g_class, (method)obj_process, "anything", A_GIMME, 0);
    class_addmethod(g_class, (method)obj_assist, "assist", A_CANT, 0);

    using attrs = avnd::attribute_input_introspection<T>;

    if constexpr(attrs::size > 0)
      attrs::for_all(attribute_register<instance, T>{g_class, attributes});

    class_register(CLASS_BOX, g_class);
  }

};

}
