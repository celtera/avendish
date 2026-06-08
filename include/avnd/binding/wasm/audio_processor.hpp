#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/wasm/callbacks.hpp>
#include <avnd/binding/wasm/ringbuffer.hpp>
#include <avnd/common/tuple.hpp>
#include <avnd/wrappers/callbacks_adapter.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/midi.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_storage.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/prepare.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/process_execution.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace wasm
{
struct audio_tick
{
  int frames{};
};

/**
 * The audio-thread half of a WASM plug-in, analogous to standalone::audio_mapper.
 * The JS AudioWorkletProcessor calls process_planar() once per quantum with
 * pointers into WASM linear memory. Lifecycle: construct -> prepare -> process.
 */
template <typename T>
struct audio_processor
{
  avnd::effect_container<T> object;
  AVND_NO_UNIQUE_ADDRESS avnd::process_adapter<T> processor;

  // Sample-accurate / MIDI scratch storage. Zero-size when unused.
  AVND_NO_UNIQUE_ADDRESS avnd::midi_storage<T> midi;
  AVND_NO_UNIQUE_ADDRESS avnd::control_storage<T> controls;

  // Callback (plug-in -> JS) machinery; events queued for JS to drain (see callbacks.hpp)
  AVND_NO_UNIQUE_ADDRESS avnd::callback_storage<T> callbacks;
#if defined(__EMSCRIPTEN__)
  callback_queue callback_events;
#endif

  int m_in_channels{};
  int m_out_channels{};
  int m_buffer_size{};
  double m_rate{};

  std::vector<float*> m_in_ptrs;
  std::vector<float*> m_out_ptrs;

  audio_processor() { avnd::init_controls(object); }

  // Pre-allocates everything so the render callback is allocation-free.
  void prepare(int in_channels, int out_channels, int buffer_size, double rate)
  {
    m_in_channels = avnd::input_channels<T>(in_channels);
    m_out_channels = avnd::output_channels<T>(out_channels);
    m_buffer_size = buffer_size;
    m_rate = rate;

    object.init_channels(m_in_channels, m_out_channels);

    avnd::process_setup setup{
        .input_channels = m_in_channels,
        .output_channels = m_out_channels,
        .frames_per_buffer = m_buffer_size,
        .rate = m_rate};

    // JS always hands us float planar buffers; allocate both so processors
    // written against either sample type work.
    processor.allocate_buffers(setup, float{});
    processor.allocate_buffers(setup, double{});

    // Reserve storage *before* avnd::prepare() so the render path stays allocation-free.
    midi.reserve_space(object, m_buffer_size);
    controls.reserve_space(object, m_buffer_size);

    // Must run before prepare(): a plug-in calling request_channels() from its own
    // prepare() would hit an empty std::function (aborts under -fno-exceptions).
    wire_request_channels();

    avnd::prepare(object, setup);

    // A variable-channel plug-in may have requested a different count in prepare().
    reconcile_variable_channels(setup);

    m_in_ptrs.assign(m_in_channels, nullptr);
    m_out_ptrs.assign(m_out_channels, nullptr);

    wire_callbacks();
  }

  // For each callback output, install a handler that queues an event for JS to
  // drain. Index is 0-based among callback outputs, matching getCallbackInfo(i).
  void wire_callbacks()
  {
#if defined(__EMSCRIPTEN__)
    using outputs_t = typename avnd::outputs_type<T>::type;
    if constexpr(avnd::callback_introspection<outputs_t>::size > 0)
    {
      auto* queue = &callback_events;
      auto initializer
          = [queue]<
                typename Field, template <typename...> typename L, typename Ret,
                typename... Args, std::size_t IdxGlob>(
                Field& /*field*/, L<Ret, Args...>, avnd::num<IdxGlob>) {
        using intro = avnd::callback_introspection<outputs_t>;
        constexpr int cb_index = (int)std::size_t(
            intro::field_index_to_index(avnd::field_index<IdxGlob>{}));
        return std::function<Ret(Args...)>{[queue](Args... args) -> Ret {
          queue->emit(cb_index, args...);
          if constexpr(!std::is_void_v<Ret>)
            return Ret{};
        }};
      };
      callbacks.wrap_callbacks(object, initializer);
    }
#endif
  }

  // Record the requested channel count on each variable_poly_audio_port; without
  // this the request_channels std::function is empty and calling it aborts.
  void wire_request_channels()
  {
    using in_bus = avnd::audio_bus_input_introspection<T>;
    using out_bus = avnd::audio_bus_output_introspection<T>;

    auto wire = [](auto& port) {
      using port_t = std::decay_t<decltype(port)>;
      if constexpr(avnd::variable_poly_audio_port<port_t>)
      {
        port.request_channels = [&port](int x) { port.channels = x; };
      }
    };
    if constexpr(in_bus::size > 0)
      in_bus::for_all(avnd::get_inputs(object), wire);
    if constexpr(out_bus::size > 0)
      out_bus::for_all(avnd::get_outputs(object), wire);
  }

  // Grow channel counts and buffers to whatever a variable bus settled on in
  // prepare(), so process_planar never indexes past an allocated channel.
  void reconcile_variable_channels(avnd::process_setup setup)
  {
    using in_bus = avnd::audio_bus_input_introspection<T>;
    using out_bus = avnd::audio_bus_output_introspection<T>;

    int in_max = m_in_channels, out_max = m_out_channels;
    if constexpr(in_bus::size > 0)
    {
      in_bus::for_all(avnd::get_inputs(object), [&](auto& port) {
        using port_t = std::decay_t<decltype(port)>;
        if constexpr(avnd::variable_poly_audio_port<port_t>)
          in_max = std::max<int>(in_max, port.channels);
      });
    }
    if constexpr(out_bus::size > 0)
    {
      out_bus::for_all(avnd::get_outputs(object), [&](auto& port) {
        using port_t = std::decay_t<decltype(port)>;
        if constexpr(avnd::variable_poly_audio_port<port_t>)
          out_max = std::max<int>(out_max, port.channels);
      });
    }

    if(in_max != m_in_channels || out_max != m_out_channels)
    {
      m_in_channels = in_max;
      m_out_channels = out_max;
      object.init_channels(m_in_channels, m_out_channels);

      setup.input_channels = m_in_channels;
      setup.output_channels = m_out_channels;
      processor.allocate_buffers(setup, float{});
      processor.allocate_buffers(setup, double{});
    }
  }

  // Clear MIDI/control in+out ports before a new block; JS then pushes events
  // into the clean input ports before calling process_planar().
  void begin_block()
  {
    midi.clear_inputs(object);
    controls.clear_inputs(object);

    // clear_inputs() shrinks the span-timed backing vectors but leaves the span
    // ports pointing at stale {data,size}; re-point them at the empty buffer.
    using span_in = avnd::span_timed_parameter_input_introspection<T>;
    if constexpr(span_in::size > 0)
    {
      span_in::for_all_n(
          avnd::get_inputs(object),
          [&]<auto SIdx, typename SField>(SField& ctl, avnd::predicate_index<SIdx>) {
        auto& buf = tpl::get<SIdx>(controls.span_timed_inputs);
        ctl.values = {buf.data(), std::size_t(0)};
      });
    }

    midi.clear_outputs(object);
    controls.clear_outputs(object);
  }

  // Process one render quantum. in_base/out_base point at planar float storage in
  // WASM memory: channel c is at base + c * frames. MIDI users call begin_block() first.
  void process_planar(float* in_base, float* out_base, int frames)
  {
    for(int c = 0; c < m_in_channels; ++c)
      m_in_ptrs[c] = in_base + (std::size_t)c * frames;
    for(int c = 0; c < m_out_channels; ++c)
      m_out_ptrs[c] = out_base + (std::size_t)c * frames;

    const audio_tick tick{.frames = frames};
    const auto ins
        = avnd::span<float*>{m_in_ptrs.data(), (std::size_t)m_in_channels};
    const auto outs
        = avnd::span<float*>{m_out_ptrs.data(), (std::size_t)m_out_channels};

    processor.process(object, ins, outs, tick);
  }

  [[nodiscard]] int input_channels() const noexcept { return m_in_channels; }
  [[nodiscard]] int output_channels() const noexcept { return m_out_channels; }
};
}
