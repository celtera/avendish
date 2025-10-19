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
#include <new>

namespace max
{
template<typename T>
struct input_matrix_storage;

template<typename T>
  requires (avnd::matrix_input_introspection<T>::size == 0)
struct input_matrix_storage<T>
{

};
template<typename T>
  requires (avnd::matrix_input_introspection<T>::size > 0)
struct input_matrix_storage<T>
{
  boost::container::vector<unsigned char> bytes[avnd::matrix_input_introspection<T>::size];
  std::array<void*, avnd::matrix_input_introspection<T>::size> matrices;
};

template<typename T>
struct output_matrix_storage;

template<typename T>
  requires (avnd::matrix_output_introspection<T>::size == 0)
struct output_matrix_storage<T>
{

};
template<typename T>
  requires (avnd::matrix_output_introspection<T>::size > 0)
struct output_matrix_storage<T>
{
  std::array<void*, avnd::matrix_output_introspection<T>::size> matrices;
};


template <typename T>
struct max_jit_wrapper;
template <typename T>
struct avnd_jit_class
{
  t_jit_object x_obj;

  max_jit_wrapper<T>* wrapper{};

  avnd::effect_container<T> implementation;

  AVND_NO_UNIQUE_ADDRESS input_matrix_storage<T> input_matrices;
  AVND_NO_UNIQUE_ADDRESS output_matrix_storage<T> output_matrices;

  static constexpr int matrix_input_count = avnd::matrix_input_introspection<T>::size;
  static constexpr int matrix_output_count = avnd::matrix_output_introspection<T>::size;

  void init()
  {
  }

  t_jit_err process(void* inputs, void* outputs)
  {
    if(!inputs || !outputs)
      return JIT_ERR_INVALID_PTR;

    // Process matrix inputs
    if constexpr(matrix_input_count > 0)
    {
      int idx = 0;
      avnd::matrix_input_introspection<T>::for_all_n2(
        avnd::get_inputs(implementation),
        [&](auto& field, auto pred_idx, auto idx) {
          read_matrix(inputs, field, idx);
        });
    }

    // Execute the implementation
    auto& proc = implementation.effect;
    if constexpr(requires { proc(); })
    {
      proc();
    }

    // Process matrix outputs
    if constexpr(matrix_output_count > 0)
    {
      avnd::matrix_output_introspection<T>::for_all_n2(
          avnd::get_outputs(implementation), [&] (auto&& field, auto pred, auto idx) { write_matrix(outputs, field, idx); });
    }

    return JIT_ERR_NONE;
  }

  template<avnd::texture_port Field, std::size_t Idx>
  void read_matrix(void* inputs, Field& field, avnd::field_index<Idx>)
  {
    // Get input matrix
    if(void* in_matrix = jit_object_method(inputs, _jit_sym_getindex, Idx))
    {
      auto& spec = max::jitter::texture_spec(field.texture);
      max::jitter::matrix_to_texture(in_matrix, field, input_matrices.bytes[Idx], spec);
    }
  }

  template<avnd::texture_port Field, std::size_t Idx>
  void write_matrix(void* outputs, Field& field, avnd::field_index<Idx>)
  {
    if(void* out_matrix = jit_object_method(outputs, _jit_sym_getindex, Idx))
    {
      if(const auto& tex = field.texture; tex.bytes && tex.width > 0 && tex.height > 0)
      {
        max::jitter::texture_to_matrix(field, out_matrix);
      }
    }
  }

  template<avnd::buffer_port Field, std::size_t Idx>
  void read_matrix(void* inputs, Field& field, avnd::field_index<Idx>)
  {
    // Get input matrix
    if(void* in_matrix = jit_object_method(inputs, _jit_sym_getindex, Idx))
    {
      max::jitter::matrix_to_buffer(in_matrix, field);
    }
  }

  template<avnd::cpu_raw_buffer_port Field, std::size_t Idx>
  void write_matrix(void* outputs, Field& field, avnd::field_index<Idx>)
  {
    if(void* out_matrix = jit_object_method(outputs, _jit_sym_getindex, Idx))
    {
      if(const auto& tex = field.buffer; tex.bytes && tex.bytesize > 0)
      {
        max::jitter::buffer_to_matrix(field, out_matrix);
      }
    }
  }

  template<avnd::cpu_typed_buffer_port Field, std::size_t Idx>
  void write_matrix(void* outputs, Field& field, avnd::field_index<Idx>)
  {
    if(void* out_matrix = jit_object_method(outputs, _jit_sym_getindex, Idx))
    {
      if(const auto& tex = field.buffer; tex.elements && tex.count> 0)
      {
        max::jitter::buffer_to_matrix(field, out_matrix);
      }
    }
  }
};

template <typename T>
struct max_jit_wrapper : processor_common<T>
{
  t_object x_obj;
  void* obex{};
  avnd_jit_class<T>* jit{};
  void* mop{};
  avnd::effect_container<T>& implementation;

  // Setup, storage...for the outputs
  AVND_NO_UNIQUE_ADDRESS inputs<T> input_setup;

  AVND_NO_UNIQUE_ADDRESS outputs<T> output_setup;

  AVND_NO_UNIQUE_ADDRESS dict_storage<T> dicts;

  max_jit_wrapper(avnd_jit_class<T>* jit, void* mop)
      : jit{jit}
      , mop{mop}
      , implementation{jit->implementation}
  {
    this->jit->wrapper = this;
  }

  void init(int argc, t_atom* argv)
  {
    /// Create internal metadata ///
    // FIXME parse dict inputs
    dicts.init(implementation);

    /// Create ports ///
    // Create inlets
    input_setup.init(implementation, x_obj);

    // Create outlets
    output_setup.init(implementation, x_obj);

    /// Initialize controls
    if constexpr(avnd::has_inputs<T>)
    {
      avnd::init_controls(implementation);
    }

    if constexpr(avnd::has_schedule<T>)
    {
      implementation.effect.schedule.schedule_at
          = [this](int64_t ts, void (*func)(T& self)) {
        t_atom a[1];
        a[0].a_type = A_LONG;
        a[0].a_w.w_long = reinterpret_cast<t_atom_long>(func);
        static_assert(sizeof(func) == sizeof(t_atom_long));
        schedule_defer(&x_obj, (method) +[] (max_jit_wrapper<T> *x, t_symbol *s, short ac, t_atom *av) {
          auto f = reinterpret_cast<decltype(func)>(av->a_w.w_long);
          f(x->implementation.effect);
        }, (long)ts, 0, 1, a);
      };
    }

    avnd::prepare(implementation, {});

    /// Pass arguments
    if constexpr(avnd::can_initialize<T>)
    {
      init_arguments<T>{}.process(implementation.effect, argc, argv);
    }

    if constexpr(avnd::attribute_input_introspection<T>::size > 0)
    {
      avnd::attribute_input_introspection<T>::for_all_n(
          avnd::get_inputs(implementation.effect),
          attribute_object_register<max_jit_wrapper<T>, T>{ &x_obj });
    }
  }

  void destroy()
  {
    dicts.release(implementation);
  }

  void process()
  {
    // Since we have MAX_JIT_MOP_FLAGS_OWN_BANG, we need to call mproc ourselves
    process_mop(this->mop);

    // Then output the matrix
    max_jit_mop_outputmatrix(this);

    // And finally the other ports
    output_setup.commit(*this);
  }

  void process_mop(void* mop)
  {
    // Get input and output lists
    void* inputlist = jit_object_method(mop, _jit_sym_getinputlist);
    void* outputlist = jit_object_method(mop, _jit_sym_getoutputlist);

    // Process via matrix_calc
    t_jit_err err = (t_jit_err)jit_object_method(
        this->jit,
        _jit_sym_matrix_calc,
        inputlist,
        outputlist);

    if(err)
      jit_error_code(this->jit, err);
  }

  template <typename V>
  void process(V value)
  {
    const int inlet = proxy_getinlet(&x_obj);

    // Process the control
    processor_common<T>::process_inlet_control(implementation, this->input_setup, inlet, value);

    if(inlet == 0)
      process();
  }

  void process_dict(t_symbol* name)
  {
    const int inlet = proxy_getinlet(&x_obj);

    // Process the control
    processor_common<T>::process_inlet_dict(implementation, this->input_setup, inlet, name);

    if(inlet == 0)
      process();
  }

  void process(t_symbol* s, int argc, t_atom* argv)
  {
    // Called when unknown symbol. We only process those
    // on the first inlet.
    const int inlet = proxy_getinlet(&x_obj);

    // First try to process messages handled explicitely in the object
    if(inlet == 0 && messages<T>{}.process_messages(implementation, s, argc, argv))
      return;
    // Then try to find if any message matches the name of a port
    if(inlet == 0 && processor_common<T>::process_inputs(implementation, s, argc, argv))
      return;

    // Then some default behaviour
    switch(argc)
    {
      case 0: // bang
      {
        if(inlet == 0)
        {
          if(s == _sym_bang)
          {
            process();
          }
          else
          {
            process_generic_message(implementation, s);
          }
          break;
        }
        [[fallthrough]];
      }
      default: {
        // Apply the data to the first inlet (other inlets are handled by Max).
        // It will always be the first parameter (attribute or not).
        processor_common<T>::process_inlet_control(implementation, this->input_setup, inlet, s, argc, argv);

        if(inlet == 0)
        {
          // Bang
          process();
        }

        break;
      }
    }
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

  static constexpr int matrix_input_count = avnd_jit_class<T>::matrix_input_count;
  static constexpr int matrix_output_count = avnd_jit_class<T>::matrix_output_count;

  static inline const auto jit_class_name = gensym(fmt::format("jit_avnd_{}", avnd::get_c_name<T>()).c_str());

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

    void* attr = nullptr;
    long attrflags = 0;

    // Create Jitter class with unique name;

    g_jit_class = (t_class*)jit_class_new(
        jit_class_name->s_name,
        (method)+[] () { return jit_new<avnd_jit_class<T>>(g_jit_class); },
        (method)+[] (avnd_jit_class<T>* x) {  std::destroy_at(x); },
        sizeof(avnd_jit_class<T>),
        0L);

    if(!g_jit_class)
      return JIT_ERR_OUT_OF_MEM;

    // Create MOP (Matrix Operator) with appropriate inputs/outputs
    if(auto mop = jit_object_new(_jit_sym_jit_mop, -1, -1))
    {
      jit_class_addadornment(g_jit_class, (t_jit_object*)mop);
    }

    // Add matrix_calc method (required for MOP)

    static constexpr auto obj_matrix_calc
        = +[](avnd_jit_class<T>* obj, void* ins, void* outs) -> void { obj->process(ins, outs); };

    jit_class_addmethod(g_jit_class, (method)obj_matrix_calc, "matrix_calc", A_CANT, 0L);

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
      auto* x = (max_jit_wrapper<T>*)max_jit_object_alloc(g_class, jit_class_name);

      if(x)
      {
        // Create jitter object (avnd_jit_class)
        if(void* o = jit_object_new(jit_class_name))
        {
          // Setup MOP extension
          max_jit_obex_jitob_set(x, o);
          // FIXME do we want a dump outlet automatically?
          max_jit_obex_dumpout_set(x, outlet_new(x, NULL));
          max_jit_mop_setup(x);

          // Create the actual C++ object (max_jit_wrapper<T>)
          {
            t_object x_obj = x->x_obj;
            void* obex = x->obex;
            auto mop = max_jit_obex_adornment_get(x, _jit_sym_jit_mop);
            assert(mop);
            auto jit = (avnd_jit_class<T>*)max_jit_obex_jitob_get(x);
            assert(jit);

            std::construct_at(x, jit, mop);
            x->x_obj = x_obj;
            x->obex = obex;
          }

          // Setup I/Os
          x->init(argc, argv);

          // Jitter & MOP finalization
          max_jit_mop_matrix_args(x, argc, argv);
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
        x->destroy();
        std::destroy_at(x);
        max_jit_mop_free(x);
        // max_jit_obex_free(x);
        max_jit_object_free(x); //  jit_object_free(max_jit_obex_jitob_get(x)); ?
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
