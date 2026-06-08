#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * MIDI bridge for the WASM backend. Timestamps are sample offsets within the
 * current block, in [0; frames).
 */

#include <avnd/binding/wasm/controls.hpp>
#include <avnd/concepts/midi.hpp>
#include <avnd/concepts/midi_port.hpp>
#include <avnd/introspection/midi.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/effect_container.hpp>

#include <algorithm>
#include <cstdint>

#if defined(__EMSCRIPTEN__)
#include <emscripten/val.h>
#endif

namespace wasm
{

#if defined(__EMSCRIPTEN__)

// Write the bytes of a JS array into an avnd midi message, clamping for the
// fixed-array case and resizing for the vector case.
template <typename Msg>
inline void assign_midi_bytes(Msg& m, const emscripten::val& bytesArr, int timestamp)
{
  m.timestamp = timestamp;

  const int n = js_is_array(bytesArr) ? bytesArr["length"].as<int>() : 0;

  if constexpr(avnd::dynamic_midi_message<Msg>)
  {
    m.bytes.clear();
    if constexpr(requires { m.bytes.reserve(n); })
      m.bytes.reserve(n);
    for(int i = 0; i < n; i++)
      m.bytes.push_back(static_cast<std::uint8_t>(bytesArr[i].as<int>() & 0xFF));
  }
  else
  {
    // Zero the remainder so stale data from a previous block can't leak in.
    const int K = (int)std::size(m.bytes);
    const int lim = std::min(n, K);
    for(int i = 0; i < lim; i++)
      m.bytes[i] = static_cast<std::uint8_t>(bytesArr[i].as<int>() & 0xFF);
    for(int i = lim; i < K; i++)
      m.bytes[i] = 0;
  }
}

template <typename Msg>
inline emscripten::val midi_bytes_to_js(const Msg& m)
{
  emscripten::val arr = emscripten::val::array();
  const int n = (int)std::size(m.bytes);
  for(int i = 0; i < n; i++)
    arr.set(i, emscripten::val((int)(std::uint8_t)m.bytes[i]));
  return arr;
}

// Push one JS event {bytes:[...], timestamp} into midi input port #portIndex
// (index among MIDI input ports only, 0..midiInputCount-1).
template <typename T>
inline void push_midi_input(
    avnd::effect_container<T>& obj, avnd::midi_storage<T>& /*midi*/, int portIndex,
    const emscripten::val& event)
{
  using midi_in = avnd::midi_input_introspection<T>;
  if constexpr(midi_in::size > 0)
  {
    int timestamp = 0;
    if(const emscripten::val ts = event["timestamp"]; !ts.isUndefined() && !ts.isNull())
      timestamp = ts.as<int>();
    const emscripten::val bytes = event["bytes"];

    int idx = 0;
    midi_in::for_all(avnd::get_inputs(obj), [&](auto& port) {
      if(idx++ != portIndex)
        return;

      using port_t = std::decay_t<decltype(port)>;
      if constexpr(avnd::raw_container_midi_port<port_t>)
      {
        using msg_t = avnd::midi_message_type<port_t>;
        msg_t m{};
        assign_midi_bytes(m, bytes, timestamp);
        port.midi_messages[port.size] = m;
        port.size++;
      }
      else if constexpr(avnd::dynamic_container_midi_port<port_t>)
      {
        using msg_t =
            typename std::decay_t<decltype(port.midi_messages)>::value_type;
        msg_t m{};
        assign_midi_bytes(m, bytes, timestamp);
        port.midi_messages.push_back(std::move(m));
      }
    });
  }
}

// Drain all midi output ports into a JS array of {port, bytes:[...], timestamp};
// `port` is the MIDI-output-port index (0..midiOutputCount-1).
template <typename T>
inline emscripten::val drain_midi_outputs(avnd::effect_container<T>& obj)
{
  emscripten::val out = emscripten::val::array();
  using midi_out = avnd::midi_output_introspection<T>;
  if constexpr(midi_out::size > 0)
  {
    int outIdx = 0;
    int portIndex = 0;
    midi_out::for_all(avnd::get_outputs(obj), [&](auto& port) {
      const int thisPort = portIndex++;
      using port_t = std::decay_t<decltype(port)>;

      auto emit = [&](const auto& m) {
        emscripten::val e = emscripten::val::object();
        e.set("port", emscripten::val(thisPort));
        e.set("bytes", midi_bytes_to_js(m));
        e.set("timestamp", emscripten::val((int)m.timestamp));
        out.set(outIdx++, std::move(e));
      };

      if constexpr(avnd::raw_container_midi_port<port_t>)
      {
        const int n = (int)port.size;
        for(int i = 0; i < n; i++)
          emit(port.midi_messages[i]);
      }
      else if constexpr(avnd::dynamic_container_midi_port<port_t>)
      {
        for(const auto& m : port.midi_messages)
          emit(m);
      }
    });
  }
  return out;
}

#endif // __EMSCRIPTEN__

template <typename T>
constexpr int midi_input_count() noexcept
{
  return (int)avnd::midi_input_introspection<T>::size;
}
template <typename T>
constexpr int midi_output_count() noexcept
{
  return (int)avnd::midi_output_introspection<T>::size;
}

} // namespace wasm
