#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/jitter_helpers.hpp>
#include <avnd/binding/max/attributes_setup.hpp>
#include <avnd/binding/max/helpers.hpp>
#include <avnd/binding/max/init.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <ext.h>
#include <ext_obex.h>
#include <jit.common.h>
#include <max.jit.mop.h>
#include <vector>
#include <memory>

namespace max
{

/**
 * Internal Jitter class for Avendish texture processors
 * This is the actual Jitter object that does the processing
 */
template <typename T>
struct avnd_jit_class
{
  // Jitter object header - MUST be first
  t_jit_object ob;

  // Our Avendish processor
  avnd::effect_container<T> processor;

  // Temporary storage for matrix data during processing
  std::vector<void*> input_matrices;
  std::vector<void*> output_matrices;

  // Count texture ports
  static constexpr int texture_input_count = avnd::cpu_texture_input_introspection<T>::size ;
  static constexpr int texture_output_count = avnd::cpu_texture_output_introspection<T>::size;

  // Static class pointer
  static inline void* s_jit_class = nullptr;

  /**
   * Register the Jitter class
   */
  static t_jit_err jit_class_init()
  {
    if(s_jit_class)
      return JIT_ERR_NONE; // Already registered

    void* mop = nullptr;
    void* attr = nullptr;
    long attrflags = 0;

    // Create Jitter class with unique name
    std::string jit_class_name = "jit_avnd_" + std::string(avnd::get_c_name<T>().data());

    s_jit_class = jit_class_new(
      jit_class_name.c_str(),
      (method)jit_class_new_imp,
      (method)jit_class_free_imp,
      sizeof(avnd_jit_class<T>),
      0L);

    if(!s_jit_class)
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
            // Set to handle char matrices (RGBA8)
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
          // Set to output char matrices (RGBA8)
          jit_attr_setsym(output, _jit_sym_type, _jit_sym_char);
          jit_attr_setlong(output, _jit_sym_planecount, 4); // RGBA
          jit_attr_setlong(output, _jit_sym_mindimcount, 2); // 2D
          jit_attr_setlong(output, _jit_sym_maxdimcount, 2); // 2D
        }
      }

      // Add MOP to class
      jit_class_addadornment(s_jit_class, (t_jit_object*)mop);
    }

    // Add matrix_calc method (required for MOP)
    jit_class_addmethod(s_jit_class, (method)matrix_calc_imp, "matrix_calc", A_CANT, 0L);

    // Add Avendish parameters as Jitter attributes
    // Note: For now we skip parameter attributes for texture processors
    // as they typically use texture inputs/outputs rather than control parameters
    // TODO: Add support for control parameters if needed

    // Register the class
    jit_class_register(s_jit_class);

    return JIT_ERR_NONE;
  }

  /**
   * Jitter class constructor
   */
  static void* jit_class_new_imp()
  {
    auto* x = (avnd_jit_class<T>*)jit_object_alloc(s_jit_class);
    if(x)
    {
      // Initialize Avendish processor
      new(&x->processor) avnd::effect_container<T>{};
      avnd::init_controls(x->processor);
      avnd::prepare(x->processor, {});

      // Allocate space for matrix pointers
      x->input_matrices.resize(texture_input_count);
      x->output_matrices.resize(texture_output_count);
    }
    return x;
  }

  /**
   * Jitter class destructor
   */
  static void jit_class_free_imp(avnd_jit_class<T>* x)
  {
    if(x)
    {
      x->processor.~effect_container();
    }
  }

  /**
   * Matrix calculation method - this is where processing happens
   */
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
    // Get the actual processor from the container
    auto& proc = x->processor.effect;

    // Call operator() on the processor
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
          if(idx < texture_output_count)
          {
            // Get output matrix
            void* out_matrix = jit_object_method(outputs, _jit_sym_getindex, idx);
            if(out_matrix)
            {
              // Convert texture to matrix
              jitter::texture_to_matrix(field, out_matrix);
            }
            idx++;
          }
        });
    }

    return JIT_ERR_NONE;
  }

  /**
   * Helper to get Jitter type symbol for a parameter
   */
  template<typename Field>
  static t_symbol* get_jitter_type_for_parameter()
  {
    using value_type = std::decay_t<decltype(Field::value)>;

    if constexpr(std::is_integral_v<value_type>)
      return _jit_sym_long;
    else if constexpr(std::is_floating_point_v<value_type>)
      return _jit_sym_float32;
    else
      return _jit_sym_atom;
  }
};

/**
 * Max wrapper class for Jitter objects
 * This wraps the Jitter class for use in Max patches
 */
template <typename T>
struct max_jit_wrapper
{
  // Max object header
  t_object ob;

  // Obex data for Jitter
  void* obex;

  // Static Max class pointer
  static inline t_class* s_max_class = nullptr;

  /**
   * Register the Max wrapper class
   */
  static void max_class_init()
  {
    t_class *jclass;

    // First ensure Jitter class is registered
    avnd_jit_class<T>::jit_class_init();

    // Create Max class
    std::string max_class_name = std::string(avnd::get_c_name<T>().data());

    s_max_class = class_new(
      max_class_name.c_str(),
      (method)max_wrapper_new,
      (method)max_wrapper_free,
      sizeof(max_jit_wrapper<T>),
      (method)nullptr,
      A_GIMME,
      0);

    // Setup obex data using new API
    max_jit_class_obex_setup(s_max_class, calcoffset(max_jit_wrapper<T>, obex));

    // Find the Jitter class
    std::string jit_class_name = "jit_avnd_" + max_class_name;
    jclass = (t_class*)jit_class_findbyname(gensym(jit_class_name.c_str()));

    // Add MOP methods using new API with flags to prevent issues
    max_jit_class_mop_wrap(s_max_class, jclass, MAX_JIT_MOP_FLAGS_OWN_BANG | MAX_JIT_MOP_FLAGS_OWN_OUTPUTMATRIX);

    // Skip standard wrap to avoid attribute recursion issues
    // Since we don't have custom attributes yet, this avoids the infinite loop
    // max_jit_class_wrap_standard(s_max_class, jclass, 0);

    // Add assist method
    class_addmethod(s_max_class, (method)max_wrapper_assist, "assist", A_CANT, 0);

    // Add message handling methods (since we use MAX_JIT_MOP_FLAGS_OWN_BANG)
    class_addmethod(s_max_class, (method)max_wrapper_bang, "bang", A_NOTHING, 0);
    class_addmethod(s_max_class, (method)max_wrapper_int, "int", A_LONG, 0);
    class_addmethod(s_max_class, (method)max_wrapper_float, "float", A_FLOAT, 0);
    class_addmethod(s_max_class, (method)max_wrapper_symbol, "symbol", A_SYM, 0);
    class_addmethod(s_max_class, (method)max_wrapper_anything, "anything", A_GIMME, 0);

    // Register the class
    class_register(CLASS_BOX, s_max_class);
  }

  /**
   * Max wrapper constructor
   */
  static void* max_wrapper_new(t_symbol* s, long argc, t_atom* argv)
  {
    std::string jit_class_name = "jit_avnd_" + std::string(avnd::get_c_name<T>().data());
    auto* x = (max_jit_wrapper<T>*)max_jit_object_alloc(s_max_class, gensym(jit_class_name.c_str()));

    if(x)
    {
      // Setup MOP with appropriate number of ins/outs
      max_jit_mop_setup_simple(x, x, argc, argv);

      // Process attributes
      max_jit_attr_args(x, argc, argv);
    }

    return x;
  }

  /**
   * Max wrapper destructor
   */
  static void max_wrapper_free(max_jit_wrapper<T>* x)
  {
    if(x)
    {
      max_jit_mop_free(x);
      max_jit_object_free(x);
    }
  }

  /**
   * Matrix processing callback for MOP
   */
  static void max_wrapper_mproc(max_jit_wrapper<T>* x, void* mop)
  {
    // Get Jitter object using new API
    void* jit_ob = max_jit_obex_jitob_get(x);
    if(!jit_ob)
      return;

    // Process via matrix_calc
    t_jit_err err = (t_jit_err)jit_object_method(
      jit_ob,
      _jit_sym_matrix_calc,
      jit_object_method(mop, _jit_sym_getinputlist),
      jit_object_method(mop, _jit_sym_getoutputlist));

    if(err)
    {
      jit_error_code(jit_ob, err);
    }
  }

  /**
   * Bang handler - triggers matrix processing
   */
  static void max_wrapper_bang(max_jit_wrapper<T>* x)
  {
    // Trigger MOP processing
    max_jit_mop_outputmatrix(x);
  }

  /**
   * Int handler
   */
  static void max_wrapper_int(max_jit_wrapper<T>* x, t_atom_long n)
  {
    // Could be used to set parameters - for now just trigger processing
    max_wrapper_bang(x);
  }

  /**
   * Float handler
   */
  static void max_wrapper_float(max_jit_wrapper<T>* x, t_atom_float f)
  {
    // Could be used to set parameters - for now just trigger processing
    max_wrapper_bang(x);
  }

  /**
   * Symbol handler
   */
  static void max_wrapper_symbol(max_jit_wrapper<T>* x, t_symbol* s)
  {
    // Could be used to set parameters - for now just trigger processing
    max_wrapper_bang(x);
  }

  /**
   * Anything handler for other messages
   */
  static void max_wrapper_anything(max_jit_wrapper<T>* x, t_symbol* s, long argc, t_atom* argv)
  {
    // Could be used for more complex messages - for now just trigger processing
    max_wrapper_bang(x);
  }

  /**
   * Inlet/outlet assist
   */
  static void max_wrapper_assist(max_jit_wrapper<T>* x, void* b, long m, long a, char* s)
  {
    if(m == ASSIST_INLET)
    {
      sprintf(s, "Matrix input %ld", a);
    }
    else
    {
      sprintf(s, "Matrix output %ld", a);
    }
  }
};

/**
 * Main metaclass that registers everything
 */
template <typename T>
struct jitter_processor_metaclass
{
  static inline jitter_processor_metaclass* instance{};

  jitter_processor_metaclass()
  {
    instance = this;

    // Register both Jitter and Max classes
    avnd_jit_class<T>::jit_class_init();
    max_jit_wrapper<T>::max_class_init();
  }
};

} // namespace max
