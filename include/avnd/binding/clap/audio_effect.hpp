#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/clap/bus_info.hpp>
#include <avnd/binding/clap/gui.hpp>
#include <avnd/binding/clap/helpers.hpp>
#include <avnd/common/export.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/midi.hpp>
#include <avnd/wrappers/control_display.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_double.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <clap/all.h>

#include <algorithm>

namespace avnd_clap
{
template <typename T>
struct midi_processor : public avnd::midi_storage<T>
{
  void
  init_midi_message(avnd::dynamic_midi_message auto& in, const clap_event_note_t& ev)
  {
    using mb = unsigned char;
    switch(ev.header.type)
    {
      case CLAP_EVENT_NOTE_ON:
        in.bytes = {mb(0x90 | ev.channel), mb(ev.key), mb(ev.velocity * 127)};
        break;
      case CLAP_EVENT_NOTE_OFF:
        in.bytes = {mb(0x80 | ev.channel), mb(ev.key), mb(ev.velocity * 127)};
        break;
      default:
        // TODO
        break;
    }
    if_possible(in.timestamp = ev.header.time);
  }
  void init_midi_message(
      avnd::dynamic_midi_message auto& in, const clap_event_midi_sysex_t& ev)
  {
    using mb = unsigned char;
    in.bytes.assign(ev.buffer, ev.buffer + ev.size);
    if_possible(in.timestamp = ev.header.time);
  }

  void
  init_midi_message(avnd::dynamic_midi_message auto& in, const clap_event_midi_t& ev)
  {
    using mb = unsigned char;
    in.bytes.assign(ev.data, ev.data + 3);
    if_possible(in.timestamp = ev.header.time);
  }

  void init_midi_message(avnd::raw_midi_message auto& in, const clap_event_note_t& ev)
  {
    using bytes_type = decltype(in.bytes);
    switch(ev.header.type)
    {
      case CLAP_EVENT_NOTE_ON:
        in.bytes[0] = 0x90 | ev.channel;
        in.bytes[1] = ev.key;
        in.bytes[2] = ev.velocity * 127;
        break;
      case CLAP_EVENT_NOTE_OFF:
        in.bytes[0] = 0x80 | ev.channel;
        in.bytes[1] = ev.key;
        in.bytes[2] = ev.velocity * 127;
        break;
      default:
        // TODO
        break;
    }
    if_possible(in.timestamp = ev.header.time);
  }

  void
  init_midi_message(avnd::raw_midi_message auto& in, const clap_event_midi_sysex_t& ev)
  {
    using bytes_type = decltype(in.bytes);
    // FIXME unsuppported
    if_possible(in.timestamp = ev.header.time);
  }

  void init_midi_message(avnd::raw_midi_message auto& in, const clap_event_midi_t& ev)
  {
    using bytes_type = decltype(in.bytes);
    static_assert(sizeof(in.bytes[0]) == sizeof(ev.data[0]));
    memcpy(std::begin(in.bytes), ev.data, 3);
    if_possible(in.timestamp = ev.header.time);
  }

  void add_message(avnd::dynamic_container_midi_port auto& port, const auto& msg)
  {
    port.midi_messages.push_back({});
    auto& elt = port.midi_messages.back();
    init_midi_message(elt, msg);
  }

  void add_message(avnd::raw_container_midi_port auto& port, const auto& msg)
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

  using editor_t = clap_editor_t<T>;
  static constexpr bool has_editor = clap_has_editor<T>;
  AVND_NO_UNIQUE_ADDRESS
  std::conditional_t<has_editor, gui_state<T, editor_t>, no_gui_state> gui;

  AVND_NO_UNIQUE_ADDRESS avnd_clap::audio_bus_info<T> audio_busses;
  AVND_NO_UNIQUE_ADDRESS avnd::process_adapter<T> processor;
  AVND_NO_UNIQUE_ADDRESS midi_processor<T> midi;

  float sample_rate{44100.};
  int buffer_size{512};

  explicit SimpleAudioEffect(const clap_host* h)
      : host{*h}
  {
    // Set-up clap data structures
    clap_plugin::desc = &descriptor;
    clap_plugin::plugin_data = this;
    clap_plugin::init = [](const struct clap_plugin* plugin) -> bool { return true; };
    clap_plugin::destroy
        = [](const struct clap_plugin* plugin) -> void { delete self(plugin); };

    clap_plugin::activate
        = [](const struct clap_plugin* plugin, double sample_rate,
             uint32_t min_frames_count, uint32_t max_frames_count) -> bool {
      auto& p = *self(plugin);
      p.sample_rate = sample_rate;
      p.buffer_size = max_frames_count;

      p.start();
      return true;
    };

    clap_plugin::deactivate = [](const struct clap_plugin* plugin) -> void {};

    clap_plugin::start_processing
        = [](const struct clap_plugin* plugin) -> bool { return true; };

    clap_plugin::stop_processing = [](const struct clap_plugin* plugin) -> void {

    };

    clap_plugin::process = [](const struct clap_plugin* plugin,
                              const clap_process* process) -> clap_process_status {
      auto& p = *self(plugin);
      p.process(*process);
      return CLAP_PROCESS_CONTINUE;
    };

    clap_plugin::get_extension
        = [](const struct clap_plugin* plugin, const char* id) -> const void* {
      auto& p = *self(plugin);

      const std::string_view id_sv = id;
      if(id_sv == "clap.params")
        return p.get_params();
      if(id_sv == "clap.audio-ports")
        return &p.audio_ports;
      if(id_sv == "clap.note-ports")
        return &p.note_ports;
      if constexpr(has_editor)
      {
        if(id_sv == CLAP_EXT_GUI)
          return get_gui();
        if(id_sv == CLAP_EXT_TIMER_SUPPORT)
          return get_timer_support();
      }

      return nullptr;
    };

    clap_plugin::on_main_thread = [](const struct clap_plugin* plugin) {};

    /// Read the initial state of the controls
    if constexpr(avnd::has_inputs<T>)
    {
      avnd::init_controls(effect);
    }

    wire_bus_queues();
  }

  // processor → UI bus: enqueue from the audio thread into a preallocated
  // lock-free queue (try_enqueue never allocates; overflow drops), drained
  // on the editor timer. Installed at construction so send_message is
  // always callable, and re-installed after editor creation since the soft
  // runtime's constructor wires a synchronous default.
  void wire_bus_queues()
  {
    if constexpr(has_editor && avnd::has_processor_to_gui_bus<T>)
    {
      if constexpr(requires { this->effect.effect.send_message = nullptr; })
      {
        this->effect.effect.send_message = [this](auto&& msg) {
          gui.bus.to_ui.queue.try_enqueue(std::forward<decltype(msg)>(msg));
        };
      }
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
    if constexpr(midi_in_info::size > 0)
    {
      midi.reserve_space(this->effect, buffer_size);
    }

    // Effect-specific preparation
    avnd::prepare(effect, setup_info);
  }

  template <auto access_samples>
  void process_impl(
      const clap_process& process, auto** inputs, int in_N, auto** outputs, int out_N)
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
        effect, avnd::span<samples_t*>{inputs, std::size_t(in_N)},
        avnd::span<samples_t*>{outputs, std::size_t(out_N)}, process.frames_count);
  }

  void process(const clap_process& process)
  {
    // Clear the control out ports
    // FIXME

    // Clear the midi out ports
    midi.clear_outputs(this->effect);

    // Process the input events
    process_in_events(process);

    // Process the audio
    {
      int in_N = avnd::input_channels<T>(2);
      int out_N = avnd::output_channels<T>(2);

      if constexpr(avnd::float_processor<T>)
      {
        auto inputs = (float**)alloca(sizeof(float*) * std::max(in_N, 1));
        auto outputs = (float**)alloca(sizeof(float*) * std::max(out_N, 1));
        // Null the slots so the zero-filled-channels invariant substitutes
        // silent buffers rather than the effect dereferencing garbage.
        std::fill_n(inputs, in_N, nullptr);
        std::fill_n(outputs, out_N, nullptr);

        process_impl<&clap_audio_buffer::data32>(process, inputs, in_N, outputs, out_N);
      }
      else if constexpr(avnd::double_processor<T>)
      {
        auto inputs = (double**)alloca(sizeof(double*) * std::max(in_N, 1));
        auto outputs = (double**)alloca(sizeof(double*) * std::max(out_N, 1));
        std::fill_n(inputs, in_N, nullptr);
        std::fill_n(outputs, out_N, nullptr);

        process_impl<&clap_audio_buffer::data64>(process, inputs, in_N, outputs, out_N);
      }
    }

    // Process the output events
    process_out_events(process);

    // Clear the control in ports
    // FIXME

    // Clear the midi in ports
    midi.clear_inputs(this->effect);
  }

  // Representative shared-controls inputs for reads / MIDI dispatch. inputs() is
  // a per-instance member_iterator for polyphonic effects, which the field
  // introspection helpers can't take; use the first instance.
  decltype(auto) controls_inputs() noexcept
  {
    if constexpr(std::is_reference_v<decltype(this->effect.inputs())>)
    {
      return (this->effect.inputs());
    }
    else
    {
      for(auto state : this->effect.full_state())
        return (state.inputs);
      static typename avnd::inputs_type<T>::type empty{};
      return (empty);
    }
  }

  void process_param(const clap_event_param_value& p)
  {
    // Apply to every (per-channel) instance; map_control_from_01 is deleted for
    // non-scalar controls, so guard it.
    for(auto state : this->effect.full_state())
      param_in_info::for_nth_mapped(
          state.inputs, p.param_id, [&]<typename C>(C& field) {
            if constexpr(requires { avnd::map_control_from_01<C>(p.value); })
              field.value = avnd::map_control_from_01<C>(p.value);
          });
  }

  void process_transport(const clap_event_transport& transport)
  {
    // TODO
  }

  void process_in_events(const clap_process& p)
  {
    // Drain UI → processor bus messages queued by the editor.
    if constexpr(has_editor && avnd::has_gui_to_processor_bus<T>)
    {
      if constexpr(requires {
                     this->effect.effect.process_message(
                         std::declval<ui_to_proc_msg_t<T>>());
                   })
      {
        ui_to_proc_msg_t<T> msg;
        while(gui.bus.to_processor.queue.try_dequeue(msg))
          this->effect.effect.process_message(std::move(msg));
      }
    }

    // Parameter and transport events apply to every plug-in; only the MIDI
    // cases depend on having MIDI inputs.
    auto N = p.in_events->size(p.in_events);

    for(uint32_t i = 0; i < N; i++)
    {
      const clap_event_header_t* ev = p.in_events->get(p.in_events, i);

      switch(ev->type)
      {
        case CLAP_EVENT_NOTE_ON:
        case CLAP_EVENT_NOTE_OFF: {
          if constexpr(midi_in_info::size > 0)
          {
            midi_in_info::for_nth_mapped(
                controls_inputs(), ((clap_event_note_t*)ev)->port_index,
                [&]<typename C>(C& in_port) {
              midi.add_message(in_port, *(clap_event_note_t*)ev);
            });
          }
          break;
        }
        case CLAP_EVENT_MIDI: {
          if constexpr(midi_in_info::size > 0)
          {
            midi_in_info::for_nth_mapped(
                controls_inputs(), ((clap_event_midi_t*)ev)->port_index,
                [&]<typename C>(C& in_port) {
              midi.add_message(in_port, *(clap_event_midi_t*)ev);
            });
          }
          break;
        }
        case CLAP_EVENT_MIDI_SYSEX: {
          if constexpr(midi_in_info::size > 0)
          {
            midi_in_info::for_nth_mapped(
                controls_inputs(), ((clap_event_midi_sysex_t*)ev)->port_index,
                [&]<typename C>(C& in_port) {
              midi.add_message(in_port, *(clap_event_midi_sysex_t*)ev);
            });
          }
          break;
        }

        case CLAP_EVENT_PARAM_VALUE: {
          process_param(*((clap_event_param_value_t*)ev));
          break;
        }
        case CLAP_EVENT_PARAM_MOD:
          break;
        case CLAP_EVENT_TRANSPORT: {
          process_transport(*((clap_event_transport_t*)ev));
          break;
        }
        case CLAP_EVENT_NOTE_CHOKE:
        case CLAP_EVENT_NOTE_EXPRESSION:
        case CLAP_EVENT_NOTE_END:
        default:
          // TODO
          break;
      }
    }
  }

  void process_out_events(const clap_process& p)
  {
    flush_gestures(p.out_events);
    // TODO module
  }

  bool get_param_info(int32_t param_index, clap_param_info* info)
  {
    if(param_index < 0 || param_index >= param_in_info::size)
      return false;

    info->id = param_in_info::index_map[param_index];
    param_in_info::for_nth_raw(
        info->id,
        [&]<std::size_t Index, typename C>(avnd::field_reflection<Index, C> field) {
      if constexpr(avnd::has_range<C>)
      {
        static constexpr auto range = avnd::get_range<C>();
        // map_control_to_double is deleted for non-scalar controls (color/xy/...)
        if constexpr(requires { avnd::map_control_to_double<C>(range.init); })
          info->default_value = avnd::map_control_to_double<C>(range.init);

        if constexpr(avnd::enum_parameter<C>)
        {
          info->min_value = 0;
          info->max_value = avnd::get_enum_choices_count<C>() - 1;
          info->flags |= CLAP_PARAM_IS_STEPPED;
        }
        else
        {
          if constexpr(requires {
                         avnd::map_control_to_double<C>(range.min);
                         avnd::map_control_to_double<C>(range.max);
                       })
          {
            info->min_value = avnd::map_control_to_double<C>(range.min);
            info->max_value = avnd::map_control_to_double<C>(range.max);
          }
          if constexpr(requires { range.step; })
            info->flags |= CLAP_PARAM_IS_STEPPED;
        }
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
        controls_inputs(), param_id, [&]<typename C>(const C& field) {
          if constexpr(requires { avnd::map_control_to_double(field); })
            *value = avnd::map_control_to_double(field);
        });

    return true;
  }

  bool get_value_text(clap_id param_id, double value, char* display, uint32_t size)
  {
    bool ok = false;
    param_in_info::for_nth_raw(
        param_id, [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C> tag) {
          if(!ok)
          {
            ok = avnd::display_control<C>(
                avnd::map_control_from_double<C>(value), display, size);
          }
        });

    return ok;
  }

  static auto self(const clap_plugin* plugin) noexcept
  {
    return reinterpret_cast<SimpleAudioEffect*>(plugin->plugin_data);
  }

  static constexpr std::array<char, 256> keywords = avnd::get_keywords<T, ';'>();
  static constexpr const auto get_features()
  {
    static constexpr bool midi_ins = avnd::midi_input_introspection<T>::size > 0;
    static constexpr bool midi_outs = avnd::midi_output_introspection<T>::size > 0;
    // Bus counts, not channel-port counts: bus-typed audio I/O
    // (dynamic_audio_bus etc.) must classify too.
    static constexpr bool audio_ins = avnd_clap::audio_bus_info<T>::input_count() > 0;
    static constexpr bool audio_outs
        = avnd_clap::audio_bus_info<T>::output_count() > 0;
    static constexpr bool instrument = midi_ins && audio_outs;
    static constexpr bool audio_fx = audio_ins && audio_outs;
    static constexpr bool analyzer = audio_ins && !audio_outs;
    static constexpr bool note_fx = !audio_ins && midi_outs;
    static constexpr bool note_detector = audio_ins && midi_outs;
    // FIXME add tags
    // FIXME add CLAP_PLUGIN_FEATURE_MONO, etc
    // FIXME combine
    if constexpr(instrument)
      return std::array<const char*, 3>{
          CLAP_PLUGIN_FEATURE_INSTRUMENT,
          audio_fx ? CLAP_PLUGIN_FEATURE_AUDIO_EFFECT : 0, 0};
    else if constexpr(audio_fx)
      return std::array<const char*, 2>{CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, 0};
    else if constexpr(note_detector)
      return std::array<const char*, 2>{CLAP_PLUGIN_FEATURE_NOTE_DETECTOR, 0};
    else if constexpr(analyzer)
      return std::array<const char*, 2>{CLAP_PLUGIN_FEATURE_ANALYZER, 0};
    else if constexpr(note_fx)
      return std::array<const char*, 2>{CLAP_PLUGIN_FEATURE_NOTE_EFFECT, 0};
    else
      // Parameter-only utility objects: hosts require one of the main
      // categories; audio-effect is the least wrong.
      return std::array<const char*, 2>{CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, 0};
  }
  static constexpr auto features = get_features();
  static constexpr clap_plugin_descriptor descriptor{
      .clap_version = CLAP_VERSION,
      .id = avnd::get_name<T>().data(),
      .name = avnd::get_name<T>().data(),
      .vendor = avnd::get_vendor<T>().data(),
      .url = avnd::get_url<T>().data(),
      .manual_url = avnd::get_manual_url<T>().data(),
      .support_url = avnd::get_support_url<T>().data(),
      .version = avnd::get_version<T>().data(),
      .description = avnd::get_description<T>().data(),
      .features = features.data()};

  // ================= GUI (clap.gui + clap.timer-support) =================
  // See binding/clap/gui.hpp for editor resolution and the gesture queue.

  bool gui_create(const char* api, bool is_floating)
  {
    if constexpr(has_editor)
    {
      if(is_floating || std::string_view{api} != clap_native_window_api)
        return false;
      gui.editor = avnd::make_ui_editor(effect);

      // The soft runtime wires the bus synchronously in its constructor;
      // replace both directions with the lock-free queues.
      if constexpr(requires { gui.editor->runtime(); })
      {
        if constexpr(avnd::has_gui_to_processor_bus<T>)
        {
          gui.editor->runtime().set_bus_to_processor([this](auto&& msg) {
            gui.bus.to_processor.queue.enqueue(std::forward<decltype(msg)>(msg));
          });
        }
        wire_bus_queues();
      }

      if(auto t = (const clap_host_timer_support*)host.get_extension(
             &host, CLAP_EXT_TIMER_SUPPORT))
        t->register_timer(&host, 16, &gui.timer_id);
      return true;
    }
    return false;
  }

  void gui_destroy()
  {
    if constexpr(has_editor)
    {
      if(gui.timer_id != CLAP_INVALID_ID)
      {
        if(auto t = (const clap_host_timer_support*)host.get_extension(
               &host, CLAP_EXT_TIMER_SUPPORT))
          t->unregister_timer(&host, gui.timer_id);
        gui.timer_id = CLAP_INVALID_ID;
      }
      if(gui.editor)
        gui.editor->close();
      gui.editor.reset();
    }
  }

  bool gui_set_parent(const clap_window* w)
  {
    if constexpr(has_editor)
    {
      if(!gui.editor || !w)
        return false;
      void* handle{};
#if defined(_WIN32)
      handle = (void*)w->win32;
#elif defined(__APPLE__)
      handle = (void*)w->cocoa;
#else
      handle = (void*)(uintptr_t)w->x11;
#endif
      gui.editor->open(
          avnd::gui_parent{handle, native_gui_api, gui.scale}, make_gui_host());
      return true;
    }
    return false;
  }

  bool gui_get_size(uint32_t* width, uint32_t* height)
  {
    if constexpr(has_editor)
    {
      if(!gui.editor)
        return false;
      const auto [w, h] = gui.editor->size();
      *width = w;
      *height = h;
      return true;
    }
    return false;
  }

  void gui_on_timer(clap_id timer_id)
  {
    if constexpr(has_editor)
    {
      if(gui.editor && timer_id == gui.timer_id)
      {
        if constexpr(avnd::has_processor_to_gui_bus<T>)
        {
          if constexpr(requires { gui.editor->runtime(); })
          {
            proc_to_ui_msg_t<T> msg;
            while(gui.bus.to_ui.queue.try_dequeue(msg))
              gui.editor->runtime().deliver_to_ui(std::move(msg));
          }
        }
        gui.editor->idle();
      }
    }
  }

  avnd::gui_host make_gui_host()
  {
    avnd::gui_host h;
    h.ctx = this;
    if constexpr(has_editor)
    {
      h.begin_edit = [](void* ctx, int param) {
        ((SimpleAudioEffect*)ctx)->push_gesture(gesture_event::begin, param);
      };
      h.perform_edit = [](void* ctx, int param, double) {
        ((SimpleAudioEffect*)ctx)->push_gesture(gesture_event::value, param);
      };
      h.end_edit = [](void* ctx, int param) {
        ((SimpleAudioEffect*)ctx)->push_gesture(gesture_event::end, param);
      };
      h.request_resize = [](void* ctx, int w, int h_) {
        auto& self = *(SimpleAudioEffect*)ctx;
        if(auto g
           = (const clap_host_gui*)self.host.get_extension(&self.host, CLAP_EXT_GUI))
          g->request_resize(&self.host, w, h_);
      };
    }
    return h;
  }

  void push_gesture(gesture_event::kind kind, int param)
  {
    if constexpr(has_editor)
    {
      if(param < 0 || param >= param_in_info::size)
        return;
      gesture_event ev{
          .type = kind, .param_id = (clap_id)param_in_info::index_map[param]};
      // The runtime writes the control before firing the hook, so the
      // current mapped value is the one to broadcast.
      if(kind == gesture_event::value)
        get_param_value(ev.param_id, &ev.value_mapped);
      gui.gestures.push(ev);

      if(auto p = (const clap_host_params*)host.get_extension(&host, CLAP_EXT_PARAMS))
        p->request_flush(&host);
    }
  }

  // Drained from params.flush() and from process(): whichever the host
  // calls first after a gesture.
  void flush_gestures(const clap_output_events_t* out)
  {
    if constexpr(has_editor)
    {
      if(!out)
        return;
      gui.gestures.drain([&](const gesture_event& ev) {
        if(ev.type == gesture_event::value)
        {
          clap_event_param_value e{};
          e.header.size = sizeof(e);
          e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
          e.header.type = CLAP_EVENT_PARAM_VALUE;
          e.param_id = ev.param_id;
          e.note_id = -1;
          e.port_index = -1;
          e.channel = -1;
          e.key = -1;
          e.value = ev.value_mapped;
          out->try_push(out, &e.header);
        }
        else
        {
          clap_event_param_gesture e{};
          e.header.size = sizeof(e);
          e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
          e.header.type = ev.type == gesture_event::begin
                              ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                              : CLAP_EVENT_PARAM_GESTURE_END;
          e.param_id = ev.param_id;
          out->try_push(out, &e.header);
        }
      });
    }
  }

  // wrapped in functions so the class is complete for the self()-> lambdas (MSVC C2027)
  static const clap_plugin_gui* get_gui() noexcept
  {
    static constexpr clap_plugin_gui gui_ext{
        .is_api_supported
        = [](const clap_plugin* plugin, const char* api, bool is_floating) -> bool {
      return !is_floating && api && std::string_view{api} == clap_native_window_api;
    },

        .get_preferred_api
        = [](const clap_plugin* plugin, const char** api, bool* is_floating) -> bool {
      *api = clap_native_window_api;
      *is_floating = false;
      return true;
    },

        .create
        = [](const clap_plugin* plugin, const char* api, bool is_floating) -> bool {
      return self(plugin)->gui_create(api, is_floating);
    },

        .destroy
        = [](const clap_plugin* plugin) -> void { self(plugin)->gui_destroy(); },

        .set_scale = [](const clap_plugin* plugin, double scale) -> bool {
      auto& p = *self(plugin);
      p.gui.scale = scale > 0. ? scale : 1.;
      // Applied at open through gui_parent.scale; forwarded live when the
      // editor supports rescaling.
      if constexpr(requires { p.gui.editor->set_scale(scale); })
      {
        if(p.gui.editor)
          p.gui.editor->set_scale(p.gui.scale);
      }
      return true;
    },

        .get_size
        = [](const clap_plugin* plugin, uint32_t* width, uint32_t* height) -> bool {
      return self(plugin)->gui_get_size(width, height);
    },

        .can_resize = [](const clap_plugin* plugin) -> bool { return false; },

        .get_resize_hints
        = [](const clap_plugin* plugin, clap_gui_resize_hints* hints) -> bool {
      return false;
    },

        .adjust_size
        = [](const clap_plugin* plugin, uint32_t* width, uint32_t* height) -> bool {
      return self(plugin)->gui_get_size(width, height);
    },

        .set_size
        = [](const clap_plugin* plugin, uint32_t width, uint32_t height) -> bool {
      uint32_t w{}, h{};
      return self(plugin)->gui_get_size(&w, &h) && w == width && h == height;
    },

        .set_parent
        = [](const clap_plugin* plugin, const clap_window* window) -> bool {
      return self(plugin)->gui_set_parent(window);
    },

        .set_transient
        = [](const clap_plugin* plugin, const clap_window* window) -> bool {
      return false;
    },

        .suggest_title = [](const clap_plugin* plugin, const char* title) -> void {},

        .show = [](const clap_plugin* plugin) -> bool { return true; },

        .hide = [](const clap_plugin* plugin) -> bool { return true; }};
    return &gui_ext;
  }

  static const clap_plugin_timer_support* get_timer_support() noexcept
  {
    static constexpr clap_plugin_timer_support timer_ext{
        .on_timer = [](const clap_plugin* plugin, clap_id timer_id) -> void {
          self(plugin)->gui_on_timer(timer_id);
        }};
    return &timer_ext;
  }

  static const clap_plugin_params* get_params() noexcept
  {
    static constexpr clap_plugin_params params{
      .count = [](const clap_plugin* plugin) -> uint32_t { return param_in_info::size; },

      .get_info = [](const clap_plugin_t* plugin, uint32_t param_index,
                     clap_param_info_t* param_info) -> bool {
    return self(plugin)->get_param_info(param_index, param_info);
  },

      .get_value
      = [](const clap_plugin* plugin, clap_id param_id, double* value) -> bool {
    return self(plugin)->get_param_value(param_id, value);
  },

      .value_to_text = [](const clap_plugin* plugin, clap_id param_id, double value,
                          char* display, uint32_t size) -> bool {
    return self(plugin)->get_value_text(param_id, value, display, size);
  },

      .text_to_value = [](const clap_plugin* plugin, clap_id param_id,
                          const char* display, double* value) -> bool { return false; },

      .flush
      = [](const clap_plugin* plugin, const clap_input_events_t* input_parameter_changes,
           const clap_output_events_t* output_parameter_changes) -> void {
    auto& p = *self(plugin);
    if(input_parameter_changes)
    {
      const auto N = input_parameter_changes->size(input_parameter_changes);
      for(uint32_t i = 0; i < N; i++)
      {
        const auto* ev
            = input_parameter_changes->get(input_parameter_changes, i);
        if(ev && ev->type == CLAP_EVENT_PARAM_VALUE)
          p.process_param(*(const clap_event_param_value*)ev);
      }
    }
    p.flush_gestures(output_parameter_changes);
  }};
    return &params;
  }

  static constexpr clap_plugin_audio_ports audio_ports{
      .count = [](const clap_plugin* plugin, bool input) -> uint32_t {
    if(input)
      return avnd_clap::audio_bus_info<T>::input_count();
    else
      return avnd_clap::audio_bus_info<T>::output_count();
  },

      .get = [](const clap_plugin* plugin, uint32_t index, bool input,
                clap_audio_port_info* info) -> bool {
    if(input)
      return avnd_clap::audio_bus_info<T>::input_info(index, *info);
    else
      return avnd_clap::audio_bus_info<T>::output_info(index, *info);
  }};

  static constexpr clap_plugin_note_ports note_ports{
      .count = [](const clap_plugin* plugin, bool input) -> uint32_t {
    if(input)
      return avnd_clap::event_bus_info<T>::input_count();
    else
      return avnd_clap::event_bus_info<T>::output_count();
  },

      .get = [](const clap_plugin* plugin, uint32_t index, bool input,
                clap_note_port_info* info) -> bool {
    if(input)
      return avnd_clap::event_bus_info<T>::input_info(index, *info);
    else
      return avnd_clap::event_bus_info<T>::output_info(index, *info);
  }};
};
}
