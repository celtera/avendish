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
#include <cmath>
#include <concurrentqueue.h>
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
struct processor_base
{
  // Head of the Pd object
  t_object x_obj;

  int64_t type_index{};
};

template <typename T>
struct message_processor : processor_base
{
  using processor_type = T;

  // Our actual code
  avnd::effect_container<T> implementation;

  // Setup, storage...for the outputs
  [[no_unique_address]] inputs<T> input_setup;

  [[no_unique_address]] outputs<T> output_setup;

  [[no_unique_address]] avnd::control_storage<T> control_buffers;

  using sched_func = void (*)(T&);
  // struct sched_event
  // {
  //   sched_func func;
  // };
  t_clock* m_clock{};

  // moodycamel::ConcurrentQueue<sched_event> funcs{};
  std::unordered_map<t_clock*, sched_func> m_clocks;
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

    // Impossible to make work reliably in Pd...
    // https://github.com/pure-data/pure-data/issues/2104
    /*
    if constexpr(avnd::has_schedule<T>)
    {
      static constexpr auto tick = +[](message_processor* x) {
        sched_event ev;
        while(x->funcs.try_dequeue(ev))
        {
          ev.func(x->implementation.effect);
        }
      };
      m_clock = clock_new(&this->x_obj, (t_method)tick);
      implementation.effect.schedule.schedule_at = [this](int64_t ts, sched_func func) {
        {
          sys_lock();
          clock_set(m_clock, 0.f);
          sys_unlock();
        }
        funcs.try_enqueue(sched_event{func});
      };
    }
*/

    if constexpr(avnd::has_clock<T>)
    {
      const std::chrono::seconds time = avnd::get_clock(this->implementation.effect);
      static constexpr auto tick = +[](message_processor* x) { x->tick_process(); };
      m_clock = clock_new(&this->x_obj, (t_method)tick);
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
      clock_set(m_clock, ms);
    }

    if constexpr(avnd::has_clocks<T>)
    {
#if 0
      static constexpr auto tick = +[](message_processor* x) {
        // sched_event ev;
        // while(x->funcs.try_dequeue(ev))
        // {
        //   ev.func(x->implementation.effect);
        // }
      };

      auto& clk_spec = avnd::get_clocks<T>(this->implementation.effect);

      implementation.effect.schedule.clock.start = [this](auto time) {
        // if(spec has clock)
        //   use it
        // else

        auto m_clock = clock_new(&this->x_obj, (t_method)tick);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
        clock_set(m_clock, ms);
      };

      implementation.effect.schedule.clock.stop = [this]() {
        t_clock* clk{};
        clock_free(clk);
      };
#endif
    }

    avnd::prepare(implementation, {});

    /// Pass arguments
    if constexpr(avnd::can_initialize<T>)
    {
      static constexpr auto init = init_arguments<T>{};
      init.process(implementation.effect, argc, argv);
    }
  }

  void destroy()
  {
    if constexpr(avnd::has_clock<T>)
    {
      clock_free(m_clock);
      m_clock = nullptr;
    }
  }

  template <typename C>
  void set_inlet(C& port, t_atom& arg)
  {
    switch(arg.a_type)
    {
      case A_FLOAT: {
        // This is the float that is supposed to go inside the first inlet if any ?
        if constexpr(std::is_arithmetic_v<decltype(port.value)> && requires {
                       port.value = 1.f;
                     })
          avnd::apply_control(port, arg.a_w.w_float);
        break;
      }

      case A_SYMBOL: {
        if constexpr(requires { port.value = "string"; })
          avnd::apply_control(port, arg.a_w.w_symbol->s_name);
        break;
      }

      default:
        break;
    }
  }

  void process_first_inlet_control(t_symbol* s, int argc, t_atom* argv)
  {
    if constexpr(avnd::has_inputs<T>)
    {
      if constexpr(avnd::parameter_input_introspection<T>::size > 0)
      {
        auto& first_inlet = avnd::pfr::get<0>(avnd::get_inputs<T>(implementation));
        if(argc > 0)
        {
          set_inlet(first_inlet, argv[0]);
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

  void tick_process()
  {
    // Do our stuff if it makes sense - some objects may not
    // even have a "processing" method
    if_possible(implementation.effect()) else if_possible(implementation.effect(1));

    // Then bang
    output_setup.commit(*this);

    // Then clean the inlets if needed
    this->control_buffers.clear_inputs(this->implementation);
  }

  void default_process(t_symbol* s, int argc, t_atom* argv)
  {
    // Potentially clear the outlets if needed
    this->control_buffers.clear_outputs(this->implementation);

    // First apply the data to the first inlet (other inlets are handled by Pd).
    process_first_inlet_control(s, argc, argv);

    tick_process();
  }

  void process(t_symbol* s, int argc, t_atom* argv)
  {
    // First try to process messages handled explicitely in the object
    if(static constexpr messages<T> messages_setup;
       messages_setup.process_messages(implementation, s, argc, argv))
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
struct message_processor_metaclass
{
  static inline t_class* g_class{};
  static inline message_processor_metaclass* instance{};

  message_processor_metaclass()
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
    constexpr auto obj_new = +[](t_symbol* s, int argc, t_atom* argv) -> void* {
      // Initializes the t_object
      t_pd* ptr = pd_new(g_class);

      // Initializes the rest
      auto obj = reinterpret_cast<instance*>(ptr);
      new(obj) instance;
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

    /// Class creation ///
    g_class = class_new(
        symbol_from_name<T>(), (t_newmethod)obj_new, (t_method)obj_free,
        sizeof(message_processor<T>), CLASS_DEFAULT, A_GIMME, 0);

    // Connect our methods
    class_addanything(g_class, (t_method)obj_process);
  }
};
template <typename T>
struct generic_processor_metaclass
{
  static inline t_class* g_class{};
  static inline generic_processor_metaclass* instance{};

  generic_processor_metaclass()
  {
    generic_processor_metaclass::instance = this;
    // using instance = message_processor<T>;
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
    // 1. We have to get all the possible types of possible constructors
    using type_list = constructor_type_list<T>;
    using processor_type_list = boost::mp11::mp_transform<message_processor, type_list>;
    static_assert(boost::mp11::mp_size<processor_type_list>{} > 0);
    static constexpr auto sz = max_sizeof(processor_type_list{});
    static constexpr auto alternatives = boost::mp11::mp_size<type_list>{};

    // Ctor
    static constexpr auto obj_new = +[](t_symbol* s, int argc, t_atom* argv) -> void* {
      // Initializes the t_object
      t_pd* ptr = pd_new(g_class);
      reinterpret_cast<processor_base*>(ptr)->type_index = -1;

      // Initializes the rest
      bool ok = false;
      pd::invoke_construct<T>(
          s, argc, argv, [&ok, &ptr, argc, argv]<typename X>(X&& new_object) {
        static constexpr int64_t index =
            typename boost::mp11::mp_find<type_list, X>::type{};
        using instance = message_processor<X>;
        auto obj = reinterpret_cast<instance*>(ptr);
        auto old_obj = obj->x_obj;

        std::construct_at(obj);

        obj->implementation.effect = std::move(new_object);
        obj->x_obj = old_obj;
        obj->type_index = index;
        obj->init(argc, argv);
        ok = true;
      });
      if(ok)
      {
        return ptr;
      }
      else
      {
        pd_free(ptr);
        return nullptr;
      }
    };

    // Dtor
    static constexpr auto for_obj = [](processor_base* obj, auto func) {
      auto do_obj_free = [=]<std::size_t I>() {
        using type = boost::mp11::mp_at_c<type_list, I>;
        using processor_type = message_processor<type>;
        auto object = static_cast<processor_type*>(obj);
        func(object);
      };
      [&]<int... Args>(std::integer_sequence<int, Args...>) {
        return (
            ((obj->type_index == Args)
             && (do_obj_free.template operator()<Args>(), true))
            || ...);
          }(std::make_integer_sequence<int, alternatives>{});
    };
    static constexpr auto obj_free = +[](processor_base* obj) -> void {
      if(obj->type_index < 0)
      {
        // If we could not construct the object
        std::destroy_at((t_pd*)obj);
      }
      else
      {
        for_obj(obj, [](auto* object) {
          object->destroy();
          std::destroy_at(object);
        });
      }
    };

    // Message processing
    constexpr auto obj_process
        = +[](processor_base* obj, t_symbol* s, int argc, t_atom* argv) -> void {
      for_obj(obj, [=](auto* object) { object->process(s, argc, argv); });
    };

    /// Class creation ///
    g_class = class_new(
        symbol_from_name<T>(), (t_newmethod)obj_new, (t_method)obj_free, sz,
        CLASS_DEFAULT, A_GIMME, 0);

    // Connect our methods
    class_addanything(g_class, (t_method)obj_process);
  }
};
}
