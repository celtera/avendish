#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/pd/helpers.hpp>
#include <avnd/binding/pd/init.hpp>
#include <avnd/binding/pd/messages.hpp>
#include <avnd/common/export.hpp>
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
 *
 * TODO: support non-audio outputs.
 */

namespace pd
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

  [[no_unique_address]] init_arguments<T> init_setup;
  [[no_unique_address]] messages<T> messages_setup;

  // we don't use ctor / dtor, because
  // this breaks aggregate-ness...
  void init(int argc, t_atom* argv)
  {
    /// Pass arguments
    if constexpr (avnd::can_initialize<T>)
    {
      init_setup.process(implementation, argc, argv);
    }

    /// Create ports ///
    // Dummy "port" used by CLASS_MAINSIGNALIN
    f = 0.f;

    // first inlet automatically created by pd
    // it receives both left channel and signals so we have to substract 1
    for (int i = 0; i < input_channels - 1; i++)
      signalinlet_new(&x_obj, 0.f);

    for (int i = 0; i < output_channels; i++)
      outlet_new(&x_obj, &s_signal);

    /// Initialize controls
    if constexpr (avnd::has_inputs<T>)
    {
      avnd::init_controls(implementation.inputs());
    }

    /// Initialize polyphony
    implementation.init_channels(input_channels, output_channels);
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
    constexpr t_perfroutine perf = +[](t_int* w)
    {
      ++w; // Arguments passed to the t_perfroutine start at 1
      return reinterpret_cast<audio_processor<T>*>(*w)->perform(w);
    };

    /// Initialize dsp_inputs
    dsp_inputs[0] = reinterpret_cast<t_int>(this);
    dsp_inputs[1] = N;

    for (int k = 0; k < input_channels + output_channels; ++k)
      dsp_inputs[2 + k] = reinterpret_cast<t_int>(sp[k]->s_vec);

    dsp_addv(perf, dsp_input_count, dsp_inputs.data());
  }

  t_int* perform(t_int* w)
  {
    const int n = (int)(*++w);

    t_sample** input{};
    if (input_channels > 0)
    {
      input = (t_sample**)alloca(sizeof(t_sample*) * input_channels);
      for (int c = 0; c < input_channels; ++c)
      {
        input[c] = (t_sample*)(*++w);
      }
    }

    t_sample** output{};
    if (output_channels > 0)
    {
      output = (t_sample**)alloca(sizeof(t_sample*) * output_channels);
      for (int c = 0; c < output_channels; ++c)
      {
        output[c] = (t_sample*)(*++w);
      }
    }

    processor.process(
        implementation,
        avnd::span<t_sample*>{input, input_channels},
        avnd::span<t_sample*>{output, output_channels},
        n);

    return ++w;
  }

  void process_inlet_control(t_symbol* s, int argc, t_atom* argv)
  {
    for(auto& state : implementation.full_state())
    {
      switch (argv[0].a_type)
      {
        case A_FLOAT:
        {
          // Note: teeeechnically, one could store a map of string -> {void*,typeid} and then cast...
          // but most pd externals seem to just do a chain of if() so this is equivalent
          float res = argv[0].a_w.w_float;
          avnd::for_each_field_ref(
              state.inputs,
              [this, s, res, &state]<typename C>(C& ctl)
              {
                if constexpr (requires { ctl.value = float{}; })
                {
                  if (avnd::get_name<C>() == s->s_name)
                  {
                    avnd::apply_control(ctl, res);
                    if_possible(ctl.update(state.effect));
                  }
                }
              });
          break;
        }

        case A_SYMBOL:
        {
          // TODO ?
          std::string res = argv[0].a_w.w_symbol->s_name;
          // thread_local for perf ?
          avnd::for_each_field_ref(
              state.inputs,
              [this, s, &res, &state](auto& ctl)
              {
                if constexpr (requires { ctl.value = std::string{}; })
                {
                  if (std::string_view{ctl.name()} == s->s_name)
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
    if (messages_setup.process_messages(implementation, s, argc, argv))
      return;

    // Then some default behaviour
    switch (argc)
    {
      case 0: // bang
      {
        if (strcmp(s->s_name, s_bang.s_name) == 0)
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
      default:
      {
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

  // DSP
  constexpr auto obj_dsp = +[](instance* obj, t_signal** sp) -> void { obj->dsp(sp); };

  // Message processing
  constexpr auto obj_process
      = +[](instance* obj, t_symbol* s, int argc, t_atom* argv) -> void
  { obj->process(s, argc, argv); };

  /// Class creation ///
  g_class = class_new(
      symbol_from_name<T>(),
      (t_newmethod)obj_new,
      (t_method)obj_free,
      sizeof(audio_processor<T>),
      CLASS_DEFAULT,
      A_GIMME,
      0);

  // First port will receive messages
  CLASS_MAINSIGNALIN(g_class, audio_processor<T>, f);

  // Connect our methods
  class_addmethod(g_class, (t_method)obj_dsp, gensym("dsp"), A_CANT, 0);
  class_addanything(g_class, (t_method)obj_process);
}

}
