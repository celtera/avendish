#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/pd/helpers.hpp>
#include <avnd/binding/pd/init.hpp>
#include <avnd/binding/pd/inputs.hpp>
#include <avnd/binding/pd/messages.hpp>
#include <avnd/binding/pd/outputs.hpp>
#include <avnd/common/export.hpp>
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/object.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <cmath>
#include <m_pd.h>

#include <cstring>
#include <span>
#include <string>

/**
 * This Pd processor is used when there is dsp processing involved.
 *
 * Inputs and outputs will be created according to the audio channel count.
 * Non-audio inputs will be processed through messages sent to the first port.
 * Non-audio outputs (e.g. an analyzer's control value) get message outlets
 * after the signal outlets; their current values are emitted on bang.
 */

namespace pd
{
template <typename T>
struct audio_processor_metaclass
{
  static inline t_class* g_class{};
  static inline audio_processor_metaclass* instance{};

  // class_name is the Pd class symbol; the setup function passes @AVND_C_NAME@ so
  // the registered class matches the external's filename (nullptr -> fall back to
  // the introspected name, for callers that construct the metaclass directly).
  audio_processor_metaclass(t_symbol* class_name = nullptr);
};

template <typename T>
struct audio_processor
{
  // Metadata
  static constexpr const int input_channels = avnd::input_channels<T>(1);
  static constexpr const int output_channels = avnd::output_channels<T>(1);

  static constexpr const int dsp_input_count
      = input_channels + output_channels + 2; // one for this, one for buffer_size

  // Head of the Pd object
  t_object x_obj;

  // Dummy float needed by CLASS_DOMAINSIGNALIN
  t_float f;

  // Our actual code
  avnd::effect_container<T> implementation;
  avnd::process_adapter<T> processor;

  std::array<t_int, dsp_input_count> dsp_inputs;
  inputs<T> input_setup;

  AVND_NO_UNIQUE_ADDRESS init_arguments<T> init_setup;
  AVND_NO_UNIQUE_ADDRESS messages<T> messages_setup;

  // Message outlets for the non-audio outputs (analyzer values, callbacks...),
  // created after the signal outlets, in port declaration order. Indexed by
  // output port index; audio ports keep a null slot.
  std::array<t_outlet*, avnd::output_introspection<T>::size> control_outlets = {};

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
    // Dummy "port" used by CLASS_MAINSIGNALIN
    f = 0.f;

    // first inlet automatically created by pd
    // it receives both left channel and signals so we have to substract 1
    for(int i = 0; i < input_channels - 1; i++)
      signalinlet_new(&x_obj, 0.f);

    for(int i = 0; i < output_channels; i++)
      outlet_new(&x_obj, &s_signal);

    // control_inputs is *not* initialized here, all the messages will go through 1st inlet.

    /// Initialize controls
    if constexpr(avnd::has_inputs<T>)
    {
      avnd::init_controls(implementation);
    }

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

    // Message outlets for the non-audio outputs, created from the TYPE-level
    // introspection so every container shape gets its outlets (multi-instance
    // per-channel objects included -- their get_outputs() is a dummy and their
    // control values cannot be committed, but the outlets must still exist so
    // patches connecting to them stay valid).
    if constexpr(avnd::output_introspection<T>::size > 0)
    {
      avnd::output_introspection<T>::for_all(
          [this]<auto Idx, typename Field>(avnd::field_reflection<Idx, Field>) {
            if constexpr(!avnd::audio_port<Field>)
              control_outlets[Idx] = outlet_new(&x_obj, symbol_for_port<Field>());
          });

      // Wire callbacks so the object can fire them during processing (only
      // possible when the container exposes single-instance outputs).
      if constexpr(requires {
                     avnd::output_introspection<T>::for_all(
                         avnd::get_outputs(implementation), [](auto&) {});
                   })
      {
        int out_k = 0;
        avnd::output_introspection<T>::for_all(
            avnd::get_outputs(implementation),
            [this, &out_k]<typename Field>(Field& ctl) {
              const int idx = out_k++;
              if constexpr(avnd::callback<Field>)
                if(control_outlets[idx])
                  outputs<T>::setup(ctl, *control_outlets[idx]);
            });
      }
    }
  }

  void destroy() { }

  void dsp(t_signal** sp)
  {
    // Initialize vectors for converting float -> double
    const int N = sp[0]->s_n;
    const float rate = sp[0]->s_sr;

    // Allocate buffers that may be required for converting float <-> double
    avnd::process_setup setup_info{
        .input_channels = input_channels,
        .output_channels = output_channels,
        .frames_per_buffer = N,
        .rate = rate};
    processor.allocate_buffers(setup_info, float{});

    // Allocate buffers if supported
    avnd::prepare(implementation, setup_info);

    // Notify puredata of the dsp execution
    constexpr t_perfroutine perf = +[](t_int* w) {
      ++w; // Arguments passed to the t_perfroutine start at 1
      return reinterpret_cast<audio_processor<T>*>(*w)->perform(w);
    };

    /// Initialize dsp_inputs
    dsp_inputs[0] = reinterpret_cast<t_int>(this);
    dsp_inputs[1] = N;

    for(int k = 0; k < input_channels + output_channels; ++k)
      dsp_inputs[2 + k] = reinterpret_cast<t_int>(sp[k]->s_vec);

    dsp_addv(perf, dsp_input_count, dsp_inputs.data());
  }

  t_int* perform(t_int* w)
  {
    const int n = (int)(*++w);

    t_sample** input{};
    if(input_channels > 0)
    {
      input = (t_sample**)alloca(sizeof(t_sample*) * input_channels);
      for(int c = 0; c < input_channels; ++c)
      {
        input[c] = (t_sample*)(*++w);
      }
    }

    t_sample** output{};
    if(output_channels > 0)
    {
      output = (t_sample**)alloca(sizeof(t_sample*) * output_channels);
      for(int c = 0; c < output_channels; ++c)
      {
        output[c] = (t_sample*)(*++w);
      }
    }

    processor.process(
        implementation, avnd::span<t_sample*>{input, input_channels},
        avnd::span<t_sample*>{output, output_channels}, n);

    // Impulse-like (optional) inputs are one-shot: consumed by this block,
    // else a single [Bang< message would fire on every subsequent block.
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

    return ++w;
  }

  // Emit the current value of every non-audio output through its outlet.
  // Called on bang: the dsp system runs operator(), so a bang after a signal
  // block flushes the control values that block produced (like [env~] etc.).
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
          avnd::get_outputs(implementation),
          [this, &out_k]<typename Field>(Field& ctl) {
            const int idx = out_k++;
            if(!control_outlets[idx])
              return;
            if constexpr(
                avnd::parameter_port<Field>
                && !avnd::sample_accurate_parameter_port<Field>)
            {
              value_to_pd_dispatch<Field>(control_outlets[idx], ctl.value);
            }
            else if constexpr(avnd::dynamic_sample_accurate_parameter_port<Field>)
            {
              for(auto& [timestamp, val] : ctl.values)
                value_to_pd_dispatch<Field>(control_outlets[idx], val);
              ctl.values.clear();
            }
          });
    }
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
      {
        if(strcmp(s->s_name, s_bang.s_name) == 0)
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
        // process_inlet_control(s, argc, argv); // -> done in input_setup.process_inputs
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

  // DSP
  static constexpr auto obj_dsp = +[](instance* obj, t_signal** sp) -> void { obj->dsp(sp); };

  // Message processing
  static constexpr auto obj_process
      = +[](instance* obj, t_symbol* s, int argc, t_atom* argv) -> void {
    obj->process(s, argc, argv);
  };

  /// Class creation ///
  g_class = class_new(
      class_name ? class_name : symbol_from_name<T>(), (t_newmethod)obj_new,
      (t_method)obj_free, sizeof(audio_processor<T>), CLASS_DEFAULT, A_GIMME, 0);

  // First port will receive messages
  CLASS_MAINSIGNALIN(g_class, audio_processor<T>, f);

  // Connect our methods
  class_addmethod(g_class, (t_method)obj_dsp, gensym("dsp"), A_CANT, 0);
  class_addanything(g_class, (t_method)obj_process);
}

}
