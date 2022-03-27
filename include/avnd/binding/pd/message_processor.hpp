#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/pd/helpers.hpp>
#include <avnd/binding/pd/init.hpp>
#include <avnd/binding/pd/inputs.hpp>
#include <avnd/binding/pd/messages.hpp>
#include <avnd/binding/pd/outputs.hpp>
#include <avnd/common/export.hpp>
#include <avnd/concepts/object.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <cmath>
#include <m_pd.h>

#include <cstring>
#include <span>
#include <string>

/**
 * This Pd processor is used when there is no dsp processing involved.
 *
 * It will create one inlet per input port.
 */
namespace pd
{
template <typename T>
struct message_processor_metaclass
{
  static inline t_class* g_class{};
  static inline message_processor_metaclass* instance{};

  message_processor_metaclass();
};

template <typename T>
struct message_processor
{
  // Head of the Pd object
  t_object x_obj;

  // Our actual code
  avnd::effect_container<T> implementation;

  // Setup, storage...for the outputs
  [[no_unique_address]] inputs<T> input_setup;

  [[no_unique_address]] outputs<T> output_setup;

  [[no_unique_address]] init_arguments<T> init_setup;

  [[no_unique_address]] messages<T> messages_setup;

  // we don't use ctor / dtor, because
  // this breaks aggregate-ness...
  void init(int argc, t_atom* argv)
  {
    /// Pass arguments
    if constexpr (avnd::can_initialize<T>)
    {
      init_setup.process(implementation.effect, argc, argv);
    }

    /// Create ports ///
    // Create inlets
    input_setup.init(implementation, x_obj);

    // Create outlets
    output_setup.init(implementation, x_obj);

    /// Initialize controls
    if constexpr (avnd::has_inputs<T>)
    {
      avnd::init_controls(implementation.inputs());
    }
  }

  void destroy() { }

  void process_first_inlet_control(t_symbol* s, int argc, t_atom* argv)
  {
    if constexpr (avnd::has_inputs<T>)
    {
      switch (argv[0].a_type)
      {
        case A_FLOAT:
        {
          // This is the float that is supposed to go inside the first inlet if any ?
          auto& first_inlet = boost::pfr::get<0>(avnd::get_inputs<T>(implementation));
          if constexpr (requires { first_inlet.value = 0.f; })
          {
            first_inlet.value = argv[0].a_w.w_float;
          }
          break;
        }

        case A_SYMBOL:
        {
          auto& first_inlet = boost::pfr::get<0>(avnd::get_inputs<T>(implementation));
          if constexpr (requires { first_inlet.value = "string"; })
          {
            first_inlet.value = argv[0].a_w.w_symbol->s_name;
          }
          break;
        }

        default:
          break;
      }
    }
  }

  void process(t_symbol* s, int argc, t_atom* argv)
  {
    // First try to process messages handled explicitely in the object
    if (messages_setup.process_messages(implementation, s, argc, argv))
      return;

    // Then some default behaviour
    switch (argc)
    {
      case 0: // bang
      {
        if (strcmp(s->s_name, s_bang.s_name) == 0)
        {
          // Do our stuff if it makes sense - some objects may not
          // even have a "processing" method
          if_possible(implementation());

          // Then bang
          output_setup.commit(implementation);
        }
        else
        {
          process_generic_message(implementation, s);
        }
        break;
      }
      default:
      {
        // First apply the data to the first inlet (other inlets are handled by Pd).
        process_first_inlet_control(s, argc, argv);

        // Do our stuff if it makes sense - some objects may not
        // even have a "processing" method
        if_possible(implementation());

        // Then bang
        output_setup.commit(implementation);

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
  static_assert(std::is_aggregate_v<T>);
  static_assert(std::is_aggregate_v<instance>);
  static_assert(std::is_nothrow_constructible_v<instance>);
  static_assert(std::is_nothrow_move_constructible_v<instance>);
  static_assert(std::is_nothrow_move_assignable_v<instance>);
#endif

  /// Small wrapper methods which will call into our actual type ///

  // Ctor
  constexpr auto obj_new = +[](t_symbol* s, int argc, t_atom* argv) -> void*
  {
    // Initializes the t_object
    t_pd* ptr = pd_new(g_class);

    // Initializes the rest
    auto obj = reinterpret_cast<instance*>(ptr);
    new (obj) instance;
    obj->init(argc, argv);
    return obj;
  };

  // Dtor
  constexpr auto obj_free = +[](instance* obj) -> void
  {
    obj->destroy();
    obj->~instance();
  };

  // Message processing
  constexpr auto obj_process
      = +[](instance* obj, t_symbol* s, int argc, t_atom* argv) -> void
  { obj->process(s, argc, argv); };

  /// Class creation ///
  g_class = class_new(
      symbol_from_name<T>(),
      (t_newmethod)obj_new,
      (t_method)obj_free,
      sizeof(message_processor<T>),
      CLASS_DEFAULT,
      A_GIMME,
      0);

  // Connect our methods
  class_addanything(g_class, (t_method)obj_process);
}

}

#define PD_DEFINE_EFFECT(EffectCName, EffectMainClass)                        \
  extern "C" AVND_EXPORTED_SYMBOL void EffectCName##_setup()                  \
  {                                                                           \
    static const pd::message_processor_metaclass<EffectMainClass> instance{}; \
  }
