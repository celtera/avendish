#pragma once
#include <avnd/wrappers/audio_channel_manager.hpp>
#include <avnd/wrappers/channels_introspection.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_double.hpp>
#include <avnd/wrappers/control_display.hpp>
#include <avnd/wrappers/messages_introspection.hpp>
#include <avnd/wrappers/midi_introspection.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <avnd/wrappers/configure.hpp>
#include <avnd/helpers/log.hpp>
#include <avnd/common/export.hpp>

#include <string_view>
#include <utility>

namespace exhs
{
struct config
{
  using logger_type = avnd::basic_logger;
};
static constexpr auto logger = avnd::basic_logger{};
}

namespace exhs
{
template<typename T>
void introspect()
{
  logger.log("loading: {}", avnd::get_name<T>());

  // Example of introspection of everything
  // See the Dump example for more comprehensive examples
  static constexpr auto print_name =
      [] <auto Idx, typename M> (avnd::field_reflection<Idx, M>) {
    logger.log(" -> introspected port: {}", avnd::get_name<M>());
    /* CUSTOMIZATION POINT */
  };
  avnd::parameter_input_introspection<T>::for_all(print_name);
  avnd::parameter_output_introspection<T>::for_all(print_name);

  avnd::midi_input_introspection<T>::for_all(print_name);
  avnd::midi_output_introspection<T>::for_all(print_name);
  avnd::raw_container_midi_output_introspection<T>::for_all(print_name);

  avnd::texture_input_introspection<T>::for_all(print_name);
  avnd::texture_output_introspection<T>::for_all(print_name);

  avnd::audio_bus_input_introspection<T>::for_all(print_name);
  avnd::audio_bus_output_introspection<T>::for_all(print_name);
  avnd::audio_channel_input_introspection<T>::for_all(print_name);
  avnd::audio_channel_output_introspection<T>::for_all(print_name);

  avnd::messages_introspection<T>::for_all(print_name);
  avnd::callback_output_introspection<T>::for_all(print_name);
}


template<typename T>
class example_processor
{
    using effect_type = T;
    avnd::effect_container<T> effect;

    using inputs_t = typename avnd::inputs_type<T>::type;
    using outputs_t = typename avnd::outputs_type<T>::type;
    using param_in_info = avnd::parameter_input_introspection<T>;
    using midi_in_info = avnd::midi_input_introspection<T>;
    using midi_out_info = avnd::midi_output_introspection<T>;

    // This allows to dedouble monophonic plug-ins.
    // This is done efficiently: as far as possible, there will be a single copy
    // of the input controls for instance, only the internal state will be duplicated
    // in memory.
    [[no_unique_address]]
    avnd::process_adapter<T> processor;

    [[no_unique_address]]
    avnd::audio_channel_manager<T> channels;

    int buffer_size{};
    double sample_rate{};

  public:
    example_processor()
      : channels{effect}
    {
      /// Print some metadata
      exhs::introspect<T>();

      /// Initialize the host with how many audio channels are requested by the plug-in,
      /// for hosts which work like this:
      {
        // Some plug-ins have fixed inputs and outputs.
        // For instance, a processor with three "mono channel" inputs - three input signals are needed.
        // If so, avnd::{in,out}put_channels<T>(X) will return 3.
        // Otherwise if the processor does not specify any fixed channel count,
        // the functions will return what's passed as their argument.

        // Note that a plug-in with dynamic output can only have those
        constexpr const int total_input_channels = avnd::input_channels<T>(-1);
        constexpr const int total_output_channels = avnd::output_channels<T>(-1);

        channels.set_input_channels(effect, 0, 2);
        channels.set_output_channels(effect, 0, 2);

        /* CUSTOMIZATION POINT */
      }

      /// Initialize the built-in presets
      if constexpr (avnd::has_programs<T>)
      {
        // Presets are stored in T::programs, here they can be enumerated
        // to the host.
        /* CUSTOMIZATION POINT */
      }

      /// Read the initial state of the controls, e.g.
      /// for each control make sure that it is initialized to its init value if any is specified
      if constexpr (avnd::has_inputs<T>)
      {
        avnd::init_controls(effect.inputs());
      }

      // FIXME also initialize ports, channels, etc...
      // to make sure that we never have uninitialized values

      /// Initialize the callbacks for nodes which have them.
      /// They allow the plug-in to notify the host.
      /// Mostly useful for programming-language-ish things, Pd, Max, etc.
      avnd::callback_introspection<outputs_t>::for_all(
            effect.outputs(), [] <typename C> (C& cb) {
        /* CUSTOMIZATION POINT */
        using call_type = decltype(C::call);
        if constexpr (avnd::function_view_ish<call_type>)
        {
          // Generate a dummy function if we don't have anything to bind it to.
          // This version does not allocate, it's a plain old C callback.
          using func_type = decltype(*cb.call.function);
          using func_reflect = avnd::function_reflection_t<func_type>;
          cb.call.function = func_reflect::synthesize();
          cb.call.context = nullptr;
        }
        else if constexpr (avnd::function_ish<call_type>)
        {
          // Here, "cb.call" is something like std::function
          cb.call = [] (auto&&...) { };
        }
      });
    }

    void audio_configuration_changed()
    {
      // Allocate buffers, setup everything
      avnd::process_setup setup_info{
          .input_channels = channels.actual_runtime_inputs,
          .output_channels = channels.actual_runtime_outputs,
          .frames_per_buffer = buffer_size,
          .rate = sample_rate};

      // This allocates the buffers that may be used for conversion
      // if e.g. we have an API that works with doubles,
      // and a plug-in that expects floats.
      // Here for instance we allocate buffers for an host that may invoke "process" with either floats or doubles.
      processor.allocate_buffers(setup_info, float{});
      processor.allocate_buffers(setup_info, double{});

      // Initialize the channels for the effect duplicator
      effect.init_channels(setup_info.input_channels, setup_info.output_channels);

      // Setup buffers for storing MIDI messages
      if constexpr (midi_in_info::size > 0)
      {
        midi_in_info::for_all(
              this->effect.inputs(),
              [&] <avnd::midi_port C> (C& in_port) {
                // Reserve space in the midi buffers
                /* CUSTOMIZATION POINT */
        });
      }

      // FIXME Setup buffers for storing sample-accurate events & ports

      // Effect-specific preparation
      avnd::prepare(effect, setup_info);

    }

    void start(int buffer_size, double sample_rate)
    {
      this->buffer_size = buffer_size;
      this->sample_rate = sample_rate;

      audio_configuration_changed();
    }

    void apply_control(int control_id, auto value)
    {
      // Here, control_id will refer to the index of a parameter of a given type.
      // That is, if the input ports of a processor are:
      //
      // struct {
      //   audio_input a;
      //   float_slider b;
      //   midi_input c;
      //   float_slider d;
      // }
      //
      // then b has index 0 and d index 1.
      //
      // If param_in_info::for_nth_raw was used,
      // the corresponding indices would be 1 and 3 respectively.

      param_in_info::for_nth_mapped(
            this->effect.inputs(),
            control_id,
            [&] <typename C> (C& field) {
              field.value = value;
      });
    }

    void apply_midi_in(int midi_id, std::array<unsigned char, 3> bytes)
    {
      midi_in_info::for_nth_mapped(
            this->effect.inputs(),
            midi_id,
            [&] <avnd::midi_port C> (C& port) {
              // Apply the midi message:
              if constexpr(avnd::dynamic_container_midi_port<C>)
              {
                port.midi_messages.push_back({.bytes = {bytes[0], bytes[1], bytes[2]},
                                              .timestamp = 0
                });
              }
              else if constexpr(avnd::raw_container_midi_port<C>)
              {
                // TODO
              }
            });
    }

    void process_input_controls()
    {
      // Here, copy the input controls to the ports in the object
      /* CUSTOMIZATION POINT */
    }

    void process_input_events()
    {
      // Here, copy the input midi events to the relevant ports in the object
      /* CUSTOMIZATION POINT */
    }

    void process_output_controls()
    {
      // Here, copy the output controls to the ports in the object
      // (e.g. db feedback, etc...)
      /* CUSTOMIZATION POINT */
    }

    void process_output_events()
    {
      // Here, copy the output MIDI events to the ports in the object
      /* CUSTOMIZATION POINT */
    }

    template<std::floating_point Fp>
    void process(Fp** inputs, int in_N, Fp** outputs, int out_N, int frames)
    {
      if(in_N != this->channels.actual_runtime_inputs)
      {
        logger.error("Host provided incorrect input channel count: expected {}, got {}", this->channels.actual_runtime_inputs, in_N);
        return;
      }
      if(out_N != this->channels.actual_runtime_outputs)
      {
        logger.error("Host provided incorrect output channel count", this->channels.actual_runtime_outputs, out_N);
        return;
      }

      // Check if processing is to be bypassed
      if constexpr (avnd::can_bypass<T>)
      {
        for (auto& impl : effect.effects())
          if (impl.bypass)
            return;
      }

      // Process inputs of all sorts
      process_input_controls();

      process_input_events();

      // Run the actual audio processing
      processor.process(
          effect,
          avnd::span<Fp*>{inputs, std::size_t(in_N)},
          avnd::span<Fp*>{outputs, std::size_t(out_N)},
          frames);

      // Copy output events
      process_output_controls();

      process_output_events();

      // Clean up MIDI ports
      if constexpr (midi_in_info::size > 0)
      {
        midi_in_info::for_all(
              this->effect.inputs(),
              [&] <avnd::midi_port C> (C& in_port) {
                // Clear the content of the allocated buffers
              });
      }
    }

    void stop()
    {

    }

};
}
