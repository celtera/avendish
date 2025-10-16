#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/processor_common.hpp>
#include <avnd/common/export.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <cmath>

#include <span>
#include <string>

/**
 * This Max processor is used when there is no dsp processing involved.
 *
 * It will create one inlet per input port.
 */
namespace max
{
template <typename T>
struct message_processor_metaclass
{
  static inline t_class* g_class{};
  static inline message_processor_metaclass* instance{};

  using attrs = avnd::attribute_input_introspection<T>;
  static constexpr int num_attributes = attrs::size;
  static inline std::array<t_object*, num_attributes> attributes;

  static t_symbol* symbol_from_name();
  message_processor_metaclass();
};

template <typename T>
struct message_processor : processor_common<T>
{
  // Head of the Max object
  t_object x_obj;

  // Our actual code
  avnd::effect_container<T> implementation;

  // Setup, storage...for the outputs
  AVND_NO_UNIQUE_ADDRESS inputs<T> input_setup;

  AVND_NO_UNIQUE_ADDRESS outputs<T> output_setup;

  AVND_NO_UNIQUE_ADDRESS dict_storage<T> dicts;

  // we don't use ctor / dtor, because
  // this breaks aggregate-ness...
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
        schedule_defer(&x_obj, (method) +[] (t_object *x, t_symbol *s, short ac, t_atom *av) {
              auto self = reinterpret_cast<message_processor*>(x);
              auto f = reinterpret_cast<decltype(func)>(av->a_w.w_long);
              f(self->implementation.effect);
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
          attribute_object_register<message_processor, T>{ &x_obj });
    }
  }

  void destroy() {
    dicts.release(implementation);
  }

  void process()
  {
    // Do our stuff if it makes sense - some objects may not
    // even have a "processing" method
    if_possible(implementation.effect());

    // Then bang
    output_setup.commit(*this);
  }

  template <typename V>
  void process(V value)
  {
    const int inlet = proxy_getinlet(&x_obj);

    // Process the control
    processor_common<T>::process_inlet_control(this->implementation, this->input_setup, inlet, value);

    if(inlet == 0)
      process();
  }

  void process_dict(t_symbol* name)
  {
    const int inlet = proxy_getinlet(&x_obj);

    // Process the control
    processor_common<T>::process_inlet_dict(this->implementation, this->input_setup, inlet, name);

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
    if(inlet == 0 && processor_common<T>::process_inputs(this->implementation, s, argc, argv))
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
        processor_common<T>::process_inlet_control(this->implementation, this->input_setup, inlet, s, argc, argv);

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

template <typename T>
message_processor_metaclass<T>::message_processor_metaclass()
{
  message_processor_metaclass::instance = this;
  using instance = message_processor<T>;
#if !defined(_MSC_VER)
  // static_assert(std::is_aggregate_v<T>);
  // static_assert(std::is_aggregate_v<instance>);
  // static_assert(std::is_nothrow_constructible_v<instance>);
  // static_assert(std::is_nothrow_move_constructible_v<instance>);
  // static_assert(std::is_nothrow_move_assignable_v<instance>);
#endif
  /// Small wrapper methods which will call into our actual type ///

  // Ctor
  static constexpr auto obj_new = +[](t_symbol* s, int argc, t_atom* argv) -> void* {
    // Initializes the t_object
    auto* ptr = object_alloc(g_class);
    t_object tmp;
    memcpy(&tmp, ptr, sizeof(t_object));

    // Initializes the rest
    auto obj = reinterpret_cast<instance*>(ptr);
    new(obj) instance;

    memcpy(&obj->x_obj, &tmp, sizeof(t_object));

    obj->init(argc, argv);
    return obj;
  };

  // Dtor
  static constexpr auto obj_free = +[](instance* obj) -> void {
    obj->destroy();
    obj->~instance();
  };

  // Message processing
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

  /// Class creation ///
  g_class = class_new(
      avnd::get_c_name<T>().data(), (method)obj_new, (method)obj_free,
      sizeof(message_processor<T>), 0L, A_GIMME, 0);

  // Connect our methods
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

}

