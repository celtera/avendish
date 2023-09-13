#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/helpers.hpp>
#include <avnd/binding/max/init.hpp>
#include <avnd/binding/max/messages.hpp>
#include <avnd/common/export.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <cmath>
#include <ext.h>
#include <z_dsp.h>

#include <cstring>
#include <span>
#include <string>

/**
 * This Pd processor is used when there is dsp processing involved.
 *
 * Inputs and outputs will be created according to the audio channel count.
 * Non-audio inputs will be processed through messages sent to the first port.
 *
 * TODO: support non-audio outputs.
 */

namespace max
{
template <typename T>
struct audio_processor_metaclass
{
  static inline t_class* g_class{};
  static inline audio_processor_metaclass* instance{};

  audio_processor_metaclass();
};

template <typename T>
struct audio_processor
{
  // Metadata
  static constexpr const int input_channels = avnd::input_channels<T>(1);
  static constexpr const int output_channels = avnd::output_channels<T>(1);

  // Head of the Pd object
  t_pxobject x_obj;

  // Our actual code
  avnd::effect_container<T> implementation;
  avnd::process_adapter<T> processor;

  [[no_unique_address]] init_arguments<T> init_setup;
  [[no_unique_address]] messages<T> messages_setup;

  int m_runtime_input_count{};
  int m_runtime_output_count{};

  // we don't use ctor / dtor, because
  // this breaks aggregate-ness...
  void init(int argc, t_atom* argv)
  {
    /// Pass arguments
    if constexpr(avnd::can_initialize<T>)
    {
      init_setup.process(implementation, argc, argv);
    }

    /// Create ports ///
    // Audio inlet already created as we're a t_pxobject
    dsp_setup(&x_obj, 1);

    // Create an audio outlet
    if constexpr(output_channels != 0)
    {
      outlet_new(&x_obj, "multichannelsignal");
    }
    x_obj.z_misc |= Z_NO_INPLACE;
    x_obj.z_misc |= Z_MC_INLETS;

    /// Initialize controls
    avnd::init_controls(implementation);

    /// Initialize polyphony
    implementation.init_channels(input_channels, output_channels);
  }

  void destroy() { }

  void
  dsp(t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
  {
    int a_chans
        = (intptr_t)object_method(dsp64, gensym("getnuminputchannels"), &x_obj, 0);

    int input_chans = std::max(this->input_channels, a_chans);
    int output_chans = std::max(this->output_channels, input_chans);

    // Initialize vectors for converting double -> float
    const int N = maxvectorsize;
    const double rate = samplerate;

    // Allocate buffers that may be required for converting float <-> double
    avnd::process_setup setup_info{
        .input_channels = input_chans,
        .output_channels = output_chans,
        .frames_per_buffer = N,
        .rate = rate};
    processor.allocate_buffers(setup_info, double{});

    // Allocate buffers if supported
    avnd::prepare(implementation, setup_info);

    // Notify puredata of the dsp execution
    constexpr t_perfroutine64 perf
        = +[](t_object* x, t_object* dsp64, double** ins, long numins, double** outs,
              long numouts, long sampleframes, long flags, void* userparam) {
            return reinterpret_cast<audio_processor*>(x)->perform(
                ins, numins, outs, numouts, sampleframes, flags, userparam);
          };

    dsp_add64(dsp64, (t_object*)this, (t_perfroutine64)perf, 0, NULL);
  }

  void perform(
      double** ins, long numins, double** outs, long numouts, long sampleframes,
      long flags, void* userparam)
  {
    processor.process(
        implementation, avnd::span<double*>{ins, std::size_t(numins)},
        avnd::span<double*>{outs, std::size_t(numouts)}, sampleframes);
  }

  void process_inlet_control(t_symbol* s, long argc, t_atom* argv)
  {
    for(auto& state : implementation.full_state())
    {
      switch(argv[0].a_type)
      {
        case A_FLOAT: {
          // Note: teeeechnically, one could store a map of string -> {void*,typeid} and then cast...
          // but most pd externals seem to just do a chain of if() so this is equivalent
          float res = argv[0].a_w.w_float;
          avnd::for_each_field_ref(state.inputs, [s, res, &state]<typename C>(C& ctl) {
            if constexpr(
                avnd::float_parameter<C> || avnd::int_parameter<C>
                || avnd::bool_parameter<C>)
            {
              constexpr std::string_view control_name = avnd::get_name<C>();
              if(control_name == s->s_name)
              {
                avnd::apply_control(ctl, res);
                if_possible(ctl.update(state.effect));
              }
            }
          });
          break;
        }

        case A_LONG: {
          // Note: teeeechnically, one could store a map of string -> {void*,typeid} and then cast...
          // but most pd externals seem to just do a chain of if() so this is equivalent
          int res = argv[0].a_w.w_long;
          avnd::for_each_field_ref(state.inputs, [s, res, &state]<typename C>(C& ctl) {
            if constexpr(
                avnd::int_parameter<C> || avnd::float_parameter<C>
                || avnd::enum_parameter<C> || avnd::bool_parameter<C>)
            {
              constexpr std::string_view control_name = avnd::get_name<C>();
              if(control_name == s->s_name)
              {
                avnd::apply_control(ctl, res);
                post(
                    "Apply contorl :%s %s %d", s->s_name, control_name.data(),
                    ctl.value);
                if_possible(ctl.update(state.effect));
              }
            }
          });
          break;
        }

        case A_SYM: {
          // TODO ?
          std::string res = argv[0].a_w.w_sym->s_name;
          avnd::for_each_field_ref(state.inputs, [s, &res, &state]<typename C>(C& ctl) {
            if constexpr(avnd::string_parameter<C>)
            {
              constexpr std::string_view control_name = avnd::get_name<C>();
              if(control_name == s->s_name)
              {
                avnd::apply_control(ctl, std::move(res));
                if_possible(ctl.update(state.effect));
              }
            }
          });
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
    if(messages_setup.process_messages(implementation, s, argc, argv))
      return;

    // Then some default behaviour
    switch(argc)
    {
      case 0: // bang
      {
        if(strcmp(s->s_name, "bang") == 0)
        {
          // Unlike message_processor, here we don't run operator() which is
          // being run in the dsp system.

          // Just bang the outputs:
          // output_setup.commit(implementation);
        }
        else
        {
          process_generic_message(implementation, s);
        }
        break;
      }
      default: {
        // Apply the data to the inlets.
        process_inlet_control(s, argc, argv);

        // Then bang
        // output_setup.commit(implementation);

        break;
      }
    }
  }
};

template <typename T>
audio_processor_metaclass<T>::audio_processor_metaclass()
{
  audio_processor_metaclass::instance = this;
  using instance = audio_processor<T>;

#if !defined(_MSC_VER)
  //static_assert(std::is_aggregate_v<T>);
  static_assert(std::is_aggregate_v<instance>);
  static_assert(std::is_nothrow_constructible_v<instance>);
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

  // DSP
  constexpr auto obj_dsp
      = +[](instance* obj, t_object* dsp64, short* count, double samplerate,
            long maxvectorsize, long flags) -> void {
    obj->dsp(dsp64, count, samplerate, maxvectorsize, flags);
  };

  constexpr auto inputchange = +[](instance* x, long index, long count) -> long {
    if(count != x->m_runtime_input_count)
    {
      x->m_runtime_input_count = count;
      return true;
    }
    else
      return false;
  };
  constexpr auto outputcount = +[](instance* x, long index) -> long {
    // TODO check whether the outputs are fixed or dynamic
    return x->m_runtime_input_count;
  };

  // Message processing
  constexpr auto obj_process
      = +[](instance* obj, t_symbol* s, int argc, t_atom* argv) -> void {
    obj->process(s, argc, argv);
  };

  /// Class creation ///
  g_class = class_new(
      avnd::get_c_name<T>().data(), (method)obj_new, (method)obj_free,
      sizeof(audio_processor<T>), 0L, A_GIMME, 0);

  class_dspinit(g_class);
  class_register(CLASS_BOX, g_class);

  // Connect our methods
  class_addmethod(g_class, (method)obj_dsp, "dsp64", A_CANT, 0);
  class_addmethod(g_class, (method)inputchange, "inputchanged", A_CANT, 0);
  class_addmethod(g_class, (method)outputcount, "multichanneloutputs", A_CANT, 0);

  class_addmethod(g_class, (method)obj_process, "anything", A_GIMME, 0);
}

}
