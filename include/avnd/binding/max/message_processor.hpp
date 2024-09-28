#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/attributes_setup.hpp>
#include <avnd/binding/max/helpers.hpp>
#include <avnd/binding/max/dict.hpp>
#include <avnd/binding/max/from_dict.hpp>
#include <avnd/binding/max/init.hpp>
#include <avnd/binding/max/inputs.hpp>
#include <avnd/binding/max/messages.hpp>
#include <avnd/binding/max/outputs.hpp>
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
struct message_processor
{
  // Head of the Max object
  t_object x_obj;

  // Our actual code
  avnd::effect_container<T> implementation;

  // Setup, storage...for the outputs
  [[no_unique_address]] inputs<T> input_setup;

  [[no_unique_address]] outputs<T> output_setup;

  [[no_unique_address]] dict_storage<T> dicts;

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

  void process_inlet_control(int inlet, t_atom_long val)
  {
    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      auto& obj = this->implementation.effect;
      this->input_setup.for_inlet(inlet, *this, [&obj, val] <typename F>(F& field) {
        if constexpr(requires { field.value = "str"; })
        {
          if constexpr(std::is_same_v<std::string, decltype(F::value)>)
          {
            field.value = std::to_string(val);
            if_possible(field.update(obj));
          }
          // FIXME int / float -> str for string_view? need separate storage
        }
        else if constexpr(requires { field.value = 0; })
        {
          static_assert(!std::is_pointer_v<decltype(field.value)>);
          field.value = val;
          if_possible(field.update(obj));
        }
      });
    }
  }

  void process_inlet_control(int inlet, t_atom_float val)
  {
    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      auto& obj = this->implementation.effect;
      this->input_setup.for_inlet(inlet, *this, [&obj, val] <typename F>(F& field) {
        if constexpr(requires { field.value = "str"; })
        {
          if constexpr(std::is_same_v<std::string, decltype(F::value)>)
          {
            field.value = std::to_string(val);
            if_possible(field.update(obj));
          }
          // FIXME int / float -> str for string_view? need separate storage
        }
        else if constexpr(requires { field.value = 0; })
        {
          static_assert(!std::is_pointer_v<decltype(field.value)>);
          field.value = val;
          if_possible(field.update(obj));
        }
      });
    }
  }

  void process_inlet_control(int inlet, struct symbol* val)
  {
    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      auto& obj = this->implementation.effect;
      this->input_setup.for_inlet(inlet, *this, [&obj, val] <typename F>(F& field) {
        if constexpr(requires { field.value = "str"; })
        {
          static_assert(!std::is_pointer_v<decltype(field.value)>);
          field.value = val->s_name;
          if_possible(field.update(obj));
        }
        else if constexpr(requires { field.value = 1; })
        {
          if constexpr(std::floating_point<decltype(F::value)>)
          {
            field.value = std::stod(val->s_name);
            if_possible(field.update(obj));
          }
          else if constexpr(std::integral<decltype(F::value)>)
          {
            field.value = std::stoll(val->s_name);
            if_possible(field.update(obj));
          }
        }
      });
    }
  }

  void process_inlet_dict(int inlet, struct symbol* val)
  {
    if constexpr(max::dict_parameter_inputs_introspection<T>::size > 0)
    {
      auto dict = dictobj_findregistered_retain(val);
      if(!dict)
        return;

      auto& obj = this->implementation.effect;
      this->input_setup.for_inlet(inlet, *this, [&obj, dict] <typename F>(F& field) {
        if constexpr(max::dict_parameter<F>)
        {
          from_dict{}(dict, field.value);
          if_possible(field.update(obj));
        }
      });
      dictobj_release(dict);
    }
  }

  template <typename Field>
  static bool
  process_inlet_control(T& obj, Field& field, int argc, t_atom* argv)
  {
    if constexpr(convertible_to_atom_list_statically<decltype(field.value)>)
    {
      if(from_atoms{argc, argv}(field.value))
      {
        if_possible(field.update(obj));
        return true;
      }
    }
    else
    {
      // FIXME
    }
    return false;
  }

  void process_inlet_control(int inlet, t_symbol* s, int argc, t_atom* argv)
  {
    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      auto& obj = this->implementation.effect;
      this->input_setup.for_inlet(inlet, *this, [&obj, argc, argv] <typename F>(F& field) {
        process_inlet_control(obj, field, argc, argv);
      });
    }
  }

  bool process_inputs(
      avnd::effect_container<T>& implementation, t_symbol* s, int argc, t_atom* argv)
  {
    // FIXME create static pointer tables instead, implement for_each_field_function_table
    // FIXME refactor with pd too
    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      bool ok = false;
      std::string_view symname = s->s_name;
      avnd::parameter_input_introspection<T>::for_all(
          avnd::get_inputs(implementation), [&]<typename M>(M& field) {
        if(ok)
          return;
        if(symname == max::get_name_symbol<M>())
        {
          ok = process_inlet_control(implementation.effect, field, argc, argv);
        }
      });
      return ok;
    }
    return false;
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
    process_inlet_control(inlet, value);

    if(inlet == 0)
      process();
  }
  void process_dict(t_symbol* name)
  {
    const int inlet = proxy_getinlet(&x_obj);

    // Process the control
    process_inlet_dict(inlet, name);

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
    if(inlet == 0 && process_inputs(implementation, s, argc, argv))
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
        process_inlet_control(inlet, s, argc, argv);

        if(inlet == 0)
        {
          // Bang
          process();
        }

        break;
      }
    }
  }

  void get_inlet_description(long index, char *dst)
  {
    avnd::input_introspection<T>::for_nth(index, [dst] <typename Field> (const Field& port) {
      if constexpr(avnd::has_description<typename Field::type>) {
        auto str = avnd::get_description<typename Field::type>();
        strcpy(dst, str.data());
      }
    });
  }

  void get_outlet_description(long index, char *dst)
  {
    avnd::output_introspection<T>::for_nth(index, [dst] <typename Field> (const Field& port) {
      if constexpr(avnd::has_description<typename Field::type>) {
        auto str = avnd::get_description<typename Field::type>();
        strcpy(dst, str.data());
      }
    });
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
  constexpr auto obj_new = +[](t_symbol* s, int argc, t_atom* argv) -> void* {
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
  constexpr auto obj_free = +[](instance* obj) -> void {
    obj->destroy();
    obj->~instance();
  };

  // Message processing
  constexpr auto obj_process
      = +[](instance* obj, t_symbol* s, int argc, t_atom* argv) -> void {
    obj->process(s, argc, argv);
  };

  constexpr auto obj_process_bang = +[](instance* obj) -> void { obj->process(); };

  constexpr auto obj_process_int
      = +[](instance* obj, t_atom_long value) -> void { obj->process(value); };

  constexpr auto obj_process_float
      = +[](instance* obj, t_atom_float value) -> void { obj->process(value); };

  constexpr auto obj_process_sym
      = +[](instance* obj, t_symbol* value) -> void { obj->process(value); };

  constexpr auto obj_process_dict
      = +[](instance* obj, t_symbol* value) -> void { obj->process_dict(value); };

  constexpr auto obj_assist
      = +[](instance* obj, void *b, long msg, long arg, char *dst) -> void {
    switch(msg) {
      case 1:
        obj->get_inlet_description(arg, dst);
        break;
      default:
        obj->get_outlet_description(arg, dst);
        break;
    }
  };

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

