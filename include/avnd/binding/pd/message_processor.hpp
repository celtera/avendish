#pragma once
#include <iostream>

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/pd/construct.hpp>
#include <avnd/binding/pd/helpers.hpp>
#include <avnd/binding/pd/init.hpp>
#include <avnd/binding/pd/inputs.hpp>
#include <avnd/binding/pd/messages.hpp>
#include <avnd/binding/pd/outputs.hpp>
#include <avnd/common/export.hpp>
#include <avnd/concepts/object.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_storage.hpp>
#include <boost/lockfree/queue.hpp>
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

  // class_name is the Pd class symbol; the setup function passes @AVND_C_NAME@ so
  // the registered class matches the external's filename (nullptr -> fall back to
  // the introspected name, for callers that construct the metaclass directly).
  message_processor_metaclass(t_symbol* class_name = nullptr);
};

template <typename T>
struct message_processor
{
  using processor_type = T;

  // Head of the Pd object
  t_object x_obj;

  // Our actual code
  avnd::effect_container<T> implementation;

  // Setup, storage...for the outputs
  AVND_NO_UNIQUE_ADDRESS inputs<T> input_setup;

  AVND_NO_UNIQUE_ADDRESS outputs<T> output_setup;

  AVND_NO_UNIQUE_ADDRESS init_arguments<T> init_setup;

  AVND_NO_UNIQUE_ADDRESS messages<T> messages_setup;

  AVND_NO_UNIQUE_ADDRESS avnd::control_storage<T> control_buffers;

  t_clock* m_clock{};
  using sched_func = void (*)(T&);
  struct sched_event
  {
    sched_func func;
    t_clock* clock;
  };

  boost::lockfree::queue<sched_event> funcs{1024};
  static constexpr const int buffer_size = 1; // Used for control storage

  // we don't use ctor / dtor, because
  // this breaks aggregate-ness...
  void init(int argc, t_atom* argv)
  {
    /// Create ports ///
    // Create inlets
    input_setup.init(implementation, x_obj);

    // Create outlets
    output_setup.init(implementation, x_obj);

    // Reserve space for controls
    this->control_buffers.reserve_space(implementation, 16);

    /// Initialize controls
    if constexpr(avnd::has_inputs<T>)
    {
      avnd::init_controls(implementation);
    }

    if constexpr(avnd::has_schedule<T>)
    {
      implementation.effect.schedule.schedule_at = [this](int64_t ts, sched_func func) {
        auto tick = +[](message_processor* x) {
          sched_event ev;
          if(x->funcs.pop(ev))
          {
            ev.func(x->implementation.effect);
            clock_free(ev.clock);
          }
        };

        auto clk = clock_new(&this->x_obj, (t_method)tick);
        clock_set(clk, 0.f);
        funcs.push(sched_event{func, clk});
      };
    }

    // A message object has no DSP context, so rate-dependent objects (e.g.
    // tick-driven filters) get a fixed default rate. One frame per processing
    // pass (= per bang), matching the synthesized tick below.
    avnd::prepare(
        implementation,
        avnd::process_setup{
            .input_channels = 0,
            .output_channels = 0,
            .frames_per_buffer = 1,
            .rate = 44100.});

    /// Pass arguments
    if constexpr(avnd::can_initialize<T>)
    {
      init_setup.process(implementation.effect, argc, argv);
    }
  }

  void destroy() { }

  void process_first_inlet_control(t_symbol* s, int argc, t_atom* argv)
  {
    if constexpr(avnd::has_inputs<T>)
    {
      if constexpr(avnd::parameter_input_introspection<T>::size > 0)
      {
        auto& first_inlet = avnd::pfr::get<0>(avnd::get_inputs<T>(implementation));
        if(argc > 0)
        {
          this->input_setup.process_inlet_control(this->implementation.effect, first_inlet, argc, argv);
        }
        else
        {
          // Bang
          if constexpr(requires { first_inlet.values[0] = {}; })
          {
            first_inlet.values[0] = {};
          }
        }
      }
    }
  }

  void default_process(t_symbol* s, int argc, t_atom* argv)
  {
    // Potentially clear the outlets if needed
    this->control_buffers.clear_outputs(this->implementation);

    // First apply the data to the first inlet (other inlets are handled by Pd).
    process_first_inlet_control(s, argc, argv);

    // Do our stuff if it makes sense - some objects may not
    // even have a "processing" method
    if constexpr(avnd::has_tick<T>)
    {
      // Objects taking a host tick: synthesize one processing pass (frames=1,
      // matching what the golden reference generator does for them).
      typename T::tick t{};
      if constexpr(requires { t.frames; })
        t.frames = 1;
      if_possible(implementation.effect(t))
      else if_possible(implementation.effect()) else if_possible(implementation.effect(1));
    }
    else
    {
      if_possible(implementation.effect()) else if_possible(implementation.effect(1));
    }

    // Then bang
    output_setup.commit(*this);

    // Then clean the inlets if needed
    this->control_buffers.clear_inputs(this->implementation);

    // Impulse-like (optional) inputs are one-shot: they are only "present" for
    // the processing pass that consumed them, else a single [Bang< message
    // would keep firing on every subsequent processing pass.
    if constexpr(
        avnd::parameter_input_introspection<T>::size > 0
        && requires {
             avnd::parameter_input_introspection<T>::for_all(
                 avnd::get_inputs<T>(implementation), [](auto&) {});
           })
      avnd::parameter_input_introspection<T>::for_all(
          avnd::get_inputs<T>(implementation), []<typename F>(F& field) {
            if constexpr(requires { field.value.reset(); })
              field.value.reset();
          });
  }

  void process(t_symbol* s, int argc, t_atom* argv)
  {
    // First try to process messages handled explicitely in the object
    if(messages_setup.process_messages(implementation, s, argc, argv))
      return;
    if(input_setup.process_inputs(implementation, s, argc, argv))
      return;

    // Then some default behaviour
    switch(argc)
    {
      case 0: // bang
        if(strcmp(s->s_name, s_bang.s_name) == 0)
          default_process(s, argc, argv);
        else
          process_generic_message(implementation, s);
        break;
      default:
        default_process(s, argc, argv);
        break;
    }
  }
};

template <typename T>
message_processor_metaclass<T>::message_processor_metaclass(t_symbol* class_name)
{
  message_processor_metaclass::instance = this;
  using instance = message_processor<T>;
  /*
#if !defined(_MSC_VER)
  static_assert(std::is_aggregate_v<T>);
  static_assert(std::is_aggregate_v<instance>);
  static_assert(std::is_nothrow_constructible_v<instance>);
  static_assert(std::is_nothrow_move_constructible_v<instance>);
  static_assert(std::is_nothrow_move_assignable_v<instance>);
#endif
*/
  /// Small wrapper methods which will call into our actual type ///

  // Ctor
  static constexpr auto obj_new = +[](t_symbol* s, int argc, t_atom* argv) -> void* {
    // Initializes the t_object
    t_pd* ptr = pd_new(g_class);

    // Initializes the rest
    auto obj = reinterpret_cast<instance*>(ptr);
    new(obj) instance;
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

  /// Class creation ///
  g_class = class_new(
      class_name ? class_name : symbol_from_name<T>(), (t_newmethod)obj_new,
      (t_method)obj_free, sizeof(message_processor<T>), CLASS_DEFAULT, A_GIMME, 0);

  // Connect our methods
  class_addanything(g_class, (t_method)obj_process);
}

}
