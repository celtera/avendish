#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/processor_common.hpp>
#include <avnd/binding/max/helpers.hpp>
#include <avnd/binding/max/init.hpp>
#include <avnd/binding/max/messages.hpp>
#include <avnd/binding/max/outputs.hpp>
#include <avnd/common/export.hpp>
#include <avnd/concepts/audio_port.hpp>
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
 * Non-audio outputs (e.g. an analyzer's control value) get message outlets
 * after the signal outlet; their current values are emitted on bang.
 */

namespace max
{
template <typename T>
struct audio_processor_metaclass
{
  static inline t_class* g_class{};
  static inline audio_processor_metaclass* instance{};

  // class_name is the Max class symbol; ext_main passes the external's own
  // C_NAME so the registered class matches the file Max loaded.
  audio_processor_metaclass(t_symbol* class_name = nullptr);
};

template <typename T>
struct audio_processor : processor_common<T>
{
  // Metadata
  static constexpr const int input_channels = avnd::input_channels<T>(1);
  static constexpr const int output_channels = avnd::output_channels<T>(1);

  // Head of the Pd object
  t_pxobject x_obj;

  // Our actual code
  avnd::effect_container<T> implementation;
  avnd::process_adapter<T> processor;

  AVND_NO_UNIQUE_ADDRESS init_arguments<T> init_setup;
  AVND_NO_UNIQUE_ADDRESS messages<T> messages_setup;

  // Message outlets for the non-audio outputs (analyzer values, callbacks...),
  // to the right of the signal outlet. Indexed by output port index; audio
  // ports keep a null slot.
  std::array<t_outlet*, avnd::output_introspection<T>::size> control_outlets = {};

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

    // Message outlets for the non-audio outputs (analyzer values, callbacks).
    // Max creates outlets right-to-left, so they are created BEFORE the signal
    // outlet (ending up to its right), in reverse declaration order.
    // Multi-instance containers whose get_outputs() yields a dummy
    // (per-sample-port objects) are skipped.
    if constexpr(
        avnd::output_introspection<T>::size > 0
        && requires {
             avnd::output_introspection<T>::for_all(
                 avnd::get_outputs(implementation), [](auto&) {});
           })
    {
      static constexpr auto N = avnd::output_introspection<T>::size;
      std::array<bool, N> is_control{};
      int out_k = 0;
      avnd::output_introspection<T>::for_all(
          avnd::get_outputs(implementation), [&is_control, &out_k]<typename C>(C&) {
            is_control[out_k++] = !avnd::audio_port<C>;
          });
      for(int i = N; i-- > 0;)
        if(is_control[i])
          control_outlets[i] = static_cast<t_outlet*>(outlet_new(&x_obj, nullptr));

      // Wire callbacks so the object can fire them during processing.
      out_k = 0;
      avnd::output_introspection<T>::for_all(
          avnd::get_outputs(implementation), [this, &out_k]<typename C>(C& ctl) {
            const int idx = out_k++;
            if constexpr(avnd::callback<C>)
              if(control_outlets[idx])
                outputs<T>::setup(ctl, *control_outlets[idx]);
          });
    }

    // Create an audio outlet
    if constexpr(output_channels != 0)
    {
      outlet_new(&x_obj, "multichannelsignal");
    }
    x_obj.z_misc |= Z_NO_INPLACE;
    x_obj.z_misc |= Z_MC_INLETS;

    /// Initialize controls
    avnd::init_controls(implementation);

    // Host-provided callables must never be left empty: prepare()/process()
    // calling an empty std::function is a hard crash (e.g. variable_audio_bus
    // request_channels).
    if constexpr(avnd::audio_bus_input_introspection<T>::size > 0)
      avnd::audio_bus_input_introspection<T>::for_all(
          avnd::get_inputs<T>(implementation), [](auto& p) {
            if constexpr(requires { p.request_channels; })
              if(!p.request_channels)
                p.request_channels = [](int) {};
          });
    if constexpr(avnd::audio_bus_output_introspection<T>::size > 0)
      avnd::audio_bus_output_introspection<T>::for_all(
          avnd::get_outputs<T>(implementation), [](auto& p) {
            if constexpr(requires { p.request_channels; })
              if(!p.request_channels)
                p.request_channels = [](int) {};
          });

    /// Initialize polyphony
    implementation.init_channels(input_channels, output_channels);
  }

  // Emit the current value of every non-audio output through its outlet.
  // Called on bang: the dsp system runs operator(), so a bang after a signal
  // block flushes the control values that block produced.
  void commit_control_outputs()
  {
    if constexpr(
        avnd::output_introspection<T>::size > 0
        && requires {
             avnd::output_introspection<T>::for_all(
                 avnd::get_outputs(implementation), [](auto&) {});
           })
    {
      int out_k = 0;
      avnd::output_introspection<T>::for_all(
          avnd::get_outputs(implementation), [this, &out_k]<typename C>(C& ctl) {
            const int idx = out_k++;
            if(!control_outlets[idx])
              return;
            if constexpr(
                avnd::parameter_port<C> && !avnd::sample_accurate_parameter_port<C>)
            {
              value_to_max_dispatch<C>(control_outlets[idx], ctl.value);
            }
            else if constexpr(avnd::dynamic_sample_accurate_parameter_port<C>)
            {
              for(auto& [timestamp, val] : ctl.values)
                value_to_max_dispatch<C>(control_outlets[idx], val);
              ctl.values.clear();
            }
          });
    }
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

    // Impulse-like (optional) inputs are one-shot: consumed by this block,
    // else a single [Bang< message would fire on every subsequent block.
    if constexpr(
        avnd::parameter_input_introspection<T>::size > 0
        && requires {
             avnd::parameter_input_introspection<T>::for_all(
                 avnd::get_inputs(implementation), [](auto&) {});
           })
      avnd::parameter_input_introspection<T>::for_all(
          avnd::get_inputs(implementation), []<typename F>(F& field) {
            if constexpr(requires { field.value.reset(); })
              field.value.reset();
          });
  }

  void process_inlet_control(t_symbol* s, long argc, t_atom* argv)
  {
    for(auto state : implementation.full_state())
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
              static constexpr auto control_name = max::get_name_symbol<C>();
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
              static constexpr auto control_name = max::get_name_symbol<C>();
              if(control_name == s->s_name)
              {
                avnd::apply_control(ctl, res);
                // post(
                //     "Apply contorl :%s %s %d", s->s_name, control_name.data(),
                //     ctl.value);
                if_possible(ctl.update(state.effect));
              }
            }
          });
          break;
        }

        case A_SYM: {
          // TODO ?
          std::string_view res = argv[0].a_w.w_sym->s_name;
          avnd::for_each_field_ref(state.inputs, [s, &res, &state]<typename C>(C& ctl) {
            if constexpr(avnd::string_parameter<C>)
            {
              static constexpr auto control_name = max::get_name_symbol<C>();
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
          commit_control_outputs();
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
        break;
      }
    }
  }
};

template <typename T>
audio_processor_metaclass<T>::audio_processor_metaclass(t_symbol* class_name)
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

  // DSP
  static constexpr auto obj_dsp
      = +[](instance* obj, t_object* dsp64, short* count, double samplerate,
            long maxvectorsize, long flags) -> void {
    obj->dsp(dsp64, count, samplerate, maxvectorsize, flags);
  };

  static constexpr auto inputchange = +[](instance* x, long index, long count) -> long {
    if(count != x->m_runtime_input_count)
    {
      x->m_runtime_input_count = count;
      return true;
    }
    else
      return false;
  };
  static constexpr auto outputcount = +[](instance* x, long index) -> long {
    // Report the number of output channels Max must allocate. This MUST match
    // dsp()'s output_chans computation, otherwise perform() receives fewer `outs`
    // buffers than the object writes -> out-of-bounds. In particular a fixed
    // output bus (e.g. fixed_audio_bus<..., 2>) always writes its N channels even
    // when fed a mono signal, so we can never report fewer than output_channels.
    const long ins
        = std::max<long>(instance::input_channels, x->m_runtime_input_count);
    return std::max<long>(instance::output_channels, ins);
  };

  // Message processing
  static constexpr auto obj_process
      = +[](instance* obj, t_symbol* s, int argc, t_atom* argv) -> void {
    obj->process(s, argc, argv);
  };

  static constexpr auto obj_assist = processor_common<T>::obj_assist;

  /// Class creation ///
  g_class = class_new(
      class_name ? class_name->s_name : avnd::get_c_name<T>().data(),
      (method)obj_new, (method)obj_free,
      sizeof(audio_processor<T>), 0L, A_GIMME, 0);

  class_dspinit(g_class);
  class_register(CLASS_BOX, g_class);

  // Connect our methods
  class_addmethod(g_class, (method)obj_dsp, "dsp64", A_CANT, 0);
  class_addmethod(g_class, (method)inputchange, "inputchanged", A_CANT, 0);
  class_addmethod(g_class, (method)outputcount, "multichanneloutputs", A_CANT, 0);
  class_addmethod(g_class, (method)obj_assist, "assist", A_CANT, 0);

  class_addmethod(g_class, (method)obj_process, "anything", A_GIMME, 0);
}

}
