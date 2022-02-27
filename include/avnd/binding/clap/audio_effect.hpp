#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/channels_introspection.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_double.hpp>
#include <avnd/wrappers/control_display.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <avnd/wrappers/midi_introspection.hpp>

#include <avnd/common/export.hpp>

#include <avnd/binding/clap/helpers.hpp>
#include <avnd/binding/clap/bus_info.hpp>
#include <clap/all.h>
#include <iostream>

namespace avnd_clap
{
template <typename T>
struct midi_processor
    : public avnd::midi_storage<T>
{
  void init_midi_message(
      avnd::dynamic_midi_message auto& in,
      const clap_event& ev)
  {
    using mb = unsigned char;
    switch (ev.type)
    {
      case CLAP_EVENT_NOTE_ON:
        in.bytes = { mb(0x90 | ev.note.channel), mb(ev.note.key), mb(ev.note.velocity * 127) };
        break;
      case CLAP_EVENT_NOTE_OFF:
        in.bytes = { mb(0x80 | ev.note.channel), mb(ev.note.key), mb(ev.note.velocity * 127) };
        break;
      case CLAP_EVENT_MIDI:
        in.bytes.assign(ev.midi.data, ev.midi.data + 3);
        break;
      case CLAP_EVENT_MIDI_SYSEX:
        in.bytes.assign(ev.midi_sysex.buffer, ev.midi_sysex.buffer + ev.midi_sysex.size);
        break;

      default:
        // TODO
        break;
    }
    if_possible(in.timestamp = ev.time);
  }

  void init_midi_message(
      avnd::raw_midi_message auto& in,
      const clap_event& ev)
  {
    using bytes_type = decltype(in.bytes);
    switch (ev.type)
    {
      case CLAP_EVENT_NOTE_ON:
        in.bytes[0] = 0x90 | ev.note.channel;
        in.bytes[1] = ev.note.key;
        in.bytes[2] = ev.note.velocity * 127;
        break;
      case CLAP_EVENT_NOTE_OFF:
        in.bytes[0] = 0x80 | ev.note.channel;
        in.bytes[1] = ev.note.key;
        in.bytes[2] = ev.note.velocity * 127;
        break;
      case CLAP_EVENT_MIDI:
        static_assert(sizeof(in.bytes[0]) == sizeof(ev.midi.data[0]));
        memcpy(std::begin(in.bytes), ev.midi.data, 3);
        break;
      case CLAP_EVENT_MIDI_SYSEX:
        // Not supported in that case
        break;

      default:
        // TODO
        break;
    }
    if_possible(in.timestamp = ev.time);
  }

  void add_message(
      avnd::dynamic_container_midi_port auto& port,
      const clap_event& msg)
  {
    port.midi_messages.push_back({});
    auto& elt = port.midi_messages.back();
    init_midi_message(elt, msg);
  }

  void add_message(
      avnd::raw_container_midi_port auto& port,
      const clap_event& msg)
  {
    auto& elt = port.midi_messages[port.size];
    init_midi_message(elt, msg);

    port.size++;
  }
};

template <typename T>
struct SimpleAudioEffect : clap_plugin
{
  using effect_type = T;
  using inputs_t = typename avnd::inputs_type<T>::type;
  using param_in_info = avnd::parameter_input_introspection<T>;
  using midi_in_info = avnd::midi_input_introspection<T>;
  using midi_out_info = avnd::midi_output_introspection<T>;
  static const constexpr int32_t parameter_count = param_in_info::size;

  avnd::effect_container<T> effect;

  const clap_host& host;

  [[no_unique_address]] avnd_clap::audio_bus_info<T> audio_busses;
  [[no_unique_address]] avnd::process_adapter<T> processor;
  [[no_unique_address]] midi_processor<T> midi;

  float sample_rate{44100.};
  int buffer_size{512};

  explicit SimpleAudioEffect(const clap_host* h)
    : host{*h}
  {
    // Set-up clap data structures
    clap_plugin::desc = &descriptor;
    clap_plugin::plugin_data = this;
    clap_plugin::init = [] (const struct clap_plugin *plugin)
        -> bool {
      return true;
    };
    clap_plugin::destroy = [] (const struct clap_plugin *plugin)
        -> void {
      delete self(plugin);
    };

    clap_plugin::activate =
        [] (const struct clap_plugin *plugin, double sample_rate, uint32_t min_frames_count, uint32_t max_frames_count)
        -> bool {
      auto& p = *self(plugin);
      p.sample_rate = sample_rate;
      p.buffer_size = max_frames_count;

      p.start();
      return true;
    };

    clap_plugin::deactivate =
        [] (const struct clap_plugin *plugin)
        -> void {
    };

    clap_plugin::start_processing = [] (const struct clap_plugin *plugin)
        -> bool {
      return true;
    };

    clap_plugin::stop_processing = [] (const struct clap_plugin *plugin)
        -> void {

    };

    clap_plugin::process = [] (const struct clap_plugin *plugin, const clap_process *process)
        -> clap_process_status {
      auto& p = *self(plugin);
      p.process(*process);
      return CLAP_PROCESS_CONTINUE;
    };

    clap_plugin::get_extension = [] (const struct clap_plugin *plugin, const char *id)
        -> const void* {
      auto& p = *self(plugin);

      const std::string_view id_sv = id;
      if(id_sv == "clap.params")
         return &p.params;
      if(id_sv == "clap.audio-ports")
        return &p.audio_ports;
      if(id_sv == "clap.note-ports")
        return &p.note_ports;

      return nullptr;
    };

    clap_plugin::on_main_thread = [] (const struct clap_plugin *plugin) {
    };

    /// Read the initial state of the controls
    if constexpr (avnd::has_inputs<T>)
    {
      avnd::init_controls(effect.inputs());
    }
  }

  void start()
  {
    // Allocate buffers, setup everything
    avnd::process_setup setup_info{
        .input_channels = avnd::input_channels<T>(2),
        .output_channels = avnd::output_channels<T>(2),
        .frames_per_buffer = buffer_size,
        .rate = sample_rate};

    processor.allocate_buffers(setup_info, double{});
    effect.init_channels(setup_info.input_channels, setup_info.output_channels);

    // Setup buffers for storing MIDI messages
    if constexpr (midi_in_info::size > 0)
    {
      midi_in_info::for_all(
            this->effect.inputs(),
            [&] <typename C> (C& in_port) {
              midi.reserve_space(in_port, buffer_size);
      });
    }

    // Effect-specific preparation
    avnd::prepare(effect, setup_info);
  }

  template<auto access_samples>
  void process_impl(const clap_process& process, auto** inputs, int in_N, auto** outputs, int out_N)
  {
    // Note: we map everything to a span.
    // But since this API has a very good bus implementation
    // we could try to leverage directly it if possible.

    int in_i = 0;
    for(int bus = 0; bus < process.audio_inputs_count; bus++)
    {
      auto& b = process.audio_inputs[bus];
      for(int k = 0; k < b.channel_count; ++k)
      {
        if(in_i < in_N)
        {
          inputs[in_i] = (b.*access_samples)[k];
          ++in_i;
        }
      }
    }

    int out_i = 0;
    for(int bus = 0; bus < process.audio_outputs_count; bus++)
    {
      auto& b = process.audio_outputs[bus];
      for(int k = 0; k < b.channel_count; ++k)
      {
        if(out_i < out_N)
        {
          outputs[out_i] = (b.*access_samples)[k];
          ++out_i;
        }
      }
    }

    using samples_t = std::decay_t<decltype(inputs[0][0])>;
    processor.process(
        effect,
        avnd::span<samples_t*>{inputs, std::size_t(in_N)},
        avnd::span<samples_t*>{outputs, std::size_t(out_N)},
        process.frames_count);
  }

  void process(const clap_process& process)
  {
    // Process the input events
    process_in_events(process);

    // Process the audio
    {
      int in_N = avnd::input_channels<T>(2);
      int out_N = avnd::input_channels<T>(2);

      if constexpr(avnd::float_processor<T>)
      {
        auto inputs = (float**)alloca(sizeof(float*) * in_N);
        auto outputs = (float**)alloca(sizeof(float*) * out_N);

        process_impl<&clap_audio_buffer::data32>(process, inputs, in_N, outputs, out_N);
      }
      else if constexpr(avnd::double_processor<T>)
      {
        auto inputs = (double**)alloca(sizeof(double*) * in_N);
        auto outputs = (double**)alloca(sizeof(double*) * out_N);

        process_impl<&clap_audio_buffer::data64>(process, inputs, in_N, outputs, out_N);
      }
    }

    // Process the output events
    process_out_events(process);

    // Clear the midi in ports
    if constexpr (midi_in_info::size > 0)
    {
      using i_info = avnd::midi_input_introspection<T>;
      auto& in_port = boost::pfr::get<i_info::index_map[0]>(effect.inputs());

      midi.clear(in_port);
    }
  }

  void process_param(const clap_event_param_value& p)
  {
    param_in_info::for_nth_mapped(
          this->effect.inputs(),
          p.param_id,
          [&] <typename C> (C& field) {
            field.value = avnd::map_control_from_01<C>(p.value);
    });
  }

  void process_transport(const clap_event_transport& transport)
  {
    // TODO
  }

  void process_in_events(const clap_process& p)
  {
    if constexpr (midi_in_info::size > 0)
    {
      auto N = p.in_events->size(p.in_events);

      for(uint32_t i = 0; i < N; i++)
      {
        auto ev = p.in_events->get(p.in_events, i);

        switch (ev->type)
        {
          case CLAP_EVENT_NOTE_ON:
          case CLAP_EVENT_NOTE_OFF:
          {
            midi_in_info::for_nth_mapped(
                  this->effect.inputs(),
                  ev->note.port_index,
                  [&] <typename C> (C& in_port) {
                    midi.add_message(in_port, *ev);
            });
            break;
          }
          case CLAP_EVENT_MIDI:
          {
            midi_in_info::for_nth_mapped(
                  this->effect.inputs(),
                  ev->midi.port_index,
                  [&] <typename C> (C& in_port) {
                    midi.add_message(in_port, *ev);
            });
            break;
          }
          case CLAP_EVENT_MIDI_SYSEX:
          {
            midi_in_info::for_nth_mapped(
                  this->effect.inputs(),
                  ev->midi_sysex.port_index,
                  [&] <typename C> (C& in_port) {
                    midi.add_message(in_port, *ev);
            });
            break;
          }

          case CLAP_EVENT_PARAM_VALUE:
          {
            process_param(ev->param_value);
            break;
          }
            break;
          case CLAP_EVENT_PARAM_MOD:
            break;
          case CLAP_EVENT_TRANSPORT:
          {
            process_transport(ev->time_info);
            break;
          }
          case CLAP_EVENT_NOTE_CHOKE:
          case CLAP_EVENT_NOTE_EXPRESSION:
          case CLAP_EVENT_NOTE_MASK:
          default:
            // TODO
            break;
        }
      }
    }
  }

  void process_out_events(const clap_process& p)
  {
    // TODO module
  }

  bool get_param_info(int32_t param_index, clap_param_info* info)
  {
    if(param_index < 0 || param_index >= param_in_info::size)
      return false;

    info->id = param_in_info::index_map[param_index];
    param_in_info::for_nth_raw(
          info->id,
          [&] <std::size_t Index, typename C>(avnd::field_reflection<Index, C> field) {
            if constexpr(requires { avnd::get_range<C>().init; })
              info->default_value = avnd::map_control_to_double<C>(avnd::get_range<C>().init);

            if constexpr(avnd::enum_parameter<C>)
            {
              info->min_value = 0;
              info->max_value = C::choices().size() - 1;
              info->flags |= CLAP_PARAM_IS_STEPPED;
            }
            else
            {
              if constexpr(requires { avnd::get_range<C>().min; })
                info->min_value = avnd::map_control_to_double<C>(avnd::get_range<C>().min);
              if constexpr(requires { avnd::get_range<C>().max; })
                info->max_value = avnd::map_control_to_double<C>(avnd::get_range<C>().max);
              if constexpr(requires { avnd::get_range<C>().step; })
                info->flags |= CLAP_PARAM_IS_STEPPED;
            }

            copy_string(info->name, C::name());
            copy_string(info->module, "");
            // TODO module
    });
    return true;
  }

  bool get_param_value(clap_id param_id, double* value)
  {
    param_in_info::for_nth_raw(
          this->effect.inputs(),
          param_id,
          [&] <typename C> (const C& field) {
            *value = avnd::map_control_to_double(field);
          });

    return true;
  }

  bool get_value_text(clap_id param_id, double value, char *display, uint32_t size)
  {
    bool ok = false;
    param_in_info::for_nth_raw(
          param_id,
          [&] <auto Idx, typename C> (avnd::field_reflection<Idx, C> tag) {
            if(!ok)
            {
              ok = avnd::display_control<C>(
                avnd::map_control_from_double<C>(value),
                display,
                size
              );
            }
            }
          );

    return ok;
  }

  static auto self(const clap_plugin* plugin) noexcept
  { return reinterpret_cast<SimpleAudioEffect*>(plugin->plugin_data); }

  static constexpr std::array<char, 256> keywords = avnd::get_keywords<T, ';'>();
  static constexpr clap_plugin_descriptor descriptor
  {
      .clap_version= CLAP_VERSION
    , .id = avnd::get_name<T>().data()
    , .name = avnd::get_name<T>().data()
    , .vendor = avnd::get_vendor<T>().data()
    , .url = avnd::get_url<T>().data()
    , .manual_url = avnd::get_manual_url<T>().data()
    , .support_url = avnd::get_support_url<T>().data()
    , .version = avnd::get_version<T>().data()
    , .description = avnd::get_description<T>().data()
    , .keywords = keywords.data() //"analog;character;roland;moog"
    , .plugin_type =
          ((avnd::midi_input_introspection<T>::size > 0 && avnd::audio_channel_output_introspection<T>::size ) ? CLAP_PLUGIN_INSTRUMENT : 0)
        | ((avnd::audio_channel_input_introspection<T>::size > 0 && avnd::audio_channel_output_introspection<T>::size > 0) ? CLAP_PLUGIN_AUDIO_EFFECT : CLAP_PLUGIN_EVENT_EFFECT)
  };

  static constexpr clap_plugin_params params{
    .count = [] (const clap_plugin *plugin) -> uint32_t
    { return param_in_info::size; },

    .get_info = [] (const clap_plugin *plugin, int32_t param_index, clap_param_info *param_info) -> bool
    { return self(plugin)->get_param_info(param_index, param_info); },

    .get_value = [] (const clap_plugin *plugin, clap_id param_id, double *value) -> bool
    { return self(plugin)->get_param_value(param_id, value); },

    .value_to_text = [] (const clap_plugin *plugin, clap_id param_id, double value, char *display, uint32_t size) -> bool
    { return self(plugin)->get_value_text(param_id, value, display, size);},

    .text_to_value = [] (const clap_plugin *plugin, clap_id param_id, const char * display, double *value) -> bool
    { return false; },

    .flush = [] (const clap_plugin *plugin, const clap_event_list *input_parameter_changes, const clap_event_list *output_parameter_changes) -> void
    { }
  };

  static constexpr clap_plugin_audio_ports audio_ports{
    .count = [] (const clap_plugin *plugin, bool input) -> uint32_t
    {
      if(input)
        return avnd_clap::audio_bus_info<T>::input_count();
      else
        return avnd_clap::audio_bus_info<T>::output_count();
    },

    .get = [] (const clap_plugin *plugin, uint32_t index, bool input, clap_audio_port_info *info) -> bool
    {
      if(input)
        return avnd_clap::audio_bus_info<T>::input_info(index, *info);
      else
        return avnd_clap::audio_bus_info<T>::output_info(index, *info);
    }
  };

  static constexpr clap_plugin_note_ports note_ports{
    .count = [] (const clap_plugin *plugin, bool input) -> uint32_t
    {
      if(input)
        return avnd_clap::event_bus_info<T>::input_count();
      else
        return avnd_clap::event_bus_info<T>::output_count();
    },

    .get = [] (const clap_plugin *plugin, uint32_t index, bool input, clap_note_port_info *info) -> bool
    {
      if(input)
        return avnd_clap::event_bus_info<T>::input_info(index, *info);
      else
        return avnd_clap::event_bus_info<T>::output_info(index, *info);
    }
  };
};
}
