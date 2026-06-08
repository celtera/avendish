#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/wasm/audio_processor.hpp>
#include <avnd/binding/wasm/callbacks.hpp>
#include <avnd/binding/wasm/controls.hpp>
#include <avnd/binding/wasm/messages.hpp>
#include <avnd/binding/wasm/midi.hpp>
#include <avnd/binding/wasm/ringbuffer.hpp>
#include <avnd/binding/wasm/sample_accurate.hpp>
#include <avnd/binding/wasm/ui_bridge.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <cstdint>
#include <string>

#if defined(__EMSCRIPTEN__)
#include <emscripten/bind.h>
#include <emscripten/emscripten.h>
#include <emscripten/val.h>
#endif

namespace wasm
{
/**
 * The JS-facing object for one plug-in instance, one per AudioWorkletProcessor.
 * JS constructs it, calls prepare(), then process(inPtr, outPtr, frames) per
 * quantum. Heap pointers are passed as integer byte offsets (how Embind surfaces
 * raw addresses); the JS helper wraps them as Float32Array views over HEAPF32.
 */
template <typename T>
class plugin_module
{
public:
  plugin_module() = default;

  void prepare(int in_channels, int out_channels, int buffer_size, double rate)
  {
    m_impl.prepare(in_channels, out_channels, buffer_size, rate);
  }

  // inPtr/outPtr are WASM heap byte offsets to planar float buffers.
  // MIDI/sample-accurate users: beginBlock(), push events, process(), then drain.
  // No-MIDI callers can call process() directly; it opens a block implicitly.
  void process(std::uintptr_t inPtr, std::uintptr_t outPtr, int frames)
  {
    if(!m_block_open)
      m_impl.begin_block();
    m_block_open = false;

    auto* in = reinterpret_cast<float*>(inPtr);
    auto* out = reinterpret_cast<float*>(outPtr);
    m_impl.process_planar(in, out, frames);
  }

  // Open a new block, clearing MIDI/control buffers. Optional; process() opens
  // one itself when this was not called.
  void beginBlock()
  {
    m_impl.begin_block();
    m_block_open = true;
  }

  [[nodiscard]] int inputChannels() const noexcept
  {
    return m_impl.input_channels();
  }
  [[nodiscard]] int outputChannels() const noexcept
  {
    return m_impl.output_channels();
  }

  // --- static metadata accessors (available before instantiation) ---
  static std::string name() { return std::string{avnd::get_name<T>()}; }
  static std::string c_name() { return std::string{avnd::get_c_name<T>()}; }
  static std::string uuid() { return std::string{avnd::get_uuid<T>()}; }
  static std::string author()
  {
    if constexpr(requires { avnd::get_author<T>(); })
      return std::string{avnd::get_author<T>()};
    else
      return {};
  }
  static std::string version()
  {
    if constexpr(requires { avnd::get_version<T>(); })
      return std::string{avnd::get_version<T>()};
    else
      return {};
  }

  // --- control / parameter surface ---
  // Parameter ports = anything with a .value member (see controls.hpp).
  using param_intro = avnd::parameter_input_introspection<T>;
  using output_intro = avnd::output_introspection<T>;

  [[nodiscard]] int getParameterCount() const noexcept
  {
    return (int)param_intro::size;
  }

  [[nodiscard]] int getOutputCount() const noexcept
  {
    return (int)output_intro::size;
  }

#if defined(__EMSCRIPTEN__)
  // Descriptor for parameter i (0 <= i < getParameterCount()).
  emscripten::val getParameterInfo(int i)
  {
    emscripten::val out = emscripten::val::null();
    if(i < 0 || i >= (int)param_intro::size)
      return out;
    param_intro::for_nth_mapped(
        m_impl.object.inputs(), i, [&]<typename F>(F& field) {
      out = wasm::make_param_info<F>(i);
    });
    return out;
  }

  // Set parameter i from a real (un-normalized) JS value.
  void setParameter(int i, emscripten::val v)
  {
    if(i < 0 || i >= (int)param_intro::size)
      return;
    param_intro::for_nth_mapped(
        m_impl.object.inputs(), i, [&]<typename F>(F& field) {
      wasm::set_field(field, m_impl.object.effect, v);
    });
  }

  // Read parameter i back out as a JS value.
  emscripten::val getParameter(int i)
  {
    emscripten::val out = emscripten::val::null();
    if(i < 0 || i >= (int)param_intro::size)
      return out;
    param_intro::for_nth_mapped(
        m_impl.object.inputs(), i, [&]<typename F>(F& field) {
      out = wasm::get_field(field);
    });
    return out;
  }

  // Set parameter i from a normalized [0;1] value.
  void setParameterNormalized(int i, double v01)
  {
    if(i < 0 || i >= (int)param_intro::size)
      return;
    param_intro::for_nth_mapped(
        m_impl.object.inputs(), i, [&]<typename F>(F& field) {
      wasm::set_field_normalized(field, m_impl.object.effect, v01);
    });
  }

  // Read parameter i back out as a normalized [0;1] value.
  double getParameterNormalized(int i)
  {
    double out = 0.0;
    if(i < 0 || i >= (int)param_intro::size)
      return out;
    param_intro::for_nth_mapped(
        m_impl.object.inputs(), i, [&]<typename F>(F& field) {
      out = wasm::get_field_normalized(field);
    });
    return out;
  }

  // Descriptor for output i.
  emscripten::val getOutputInfo(int i)
  {
    emscripten::val out = emscripten::val::null();
    if(i < 0 || i >= (int)output_intro::size)
      return out;
    output_intro::for_nth(i, [&]<typename FR>(FR fr) {
      using F = typename FR::type;
      // Only value/control outputs carry a descriptor; audio/callbacks -> null.
      if constexpr(avnd::parameter_port<F>)
        out = wasm::make_param_info<F>(i);
    });
    return out;
  }

  // Read output i's current value as a JS value (value/control outputs only).
  emscripten::val getOutputValue(int i)
  {
    emscripten::val out = emscripten::val::null();
    if(i < 0 || i >= (int)output_intro::size)
      return out;
    // The value-form for_nth only exists when outputs are real value members;
    // type-form / operator()-argument outputs have no live value here -> null.
    using outs_t = std::decay_t<decltype(avnd::get_outputs(m_impl.object))>;
    if constexpr(requires(outs_t& o) {
                   output_intro::for_nth(o, 0, [](auto&) {});
                 })
    {
      output_intro::for_nth(
          avnd::get_outputs(m_impl.object), i, [&]<typename F>(F& field) {
        if constexpr(requires { field.value; })
          out = wasm::to_js(field.value);
      });
    }
    return out;
  }
#endif

  // ---- MIDI / message / sample-accurate counts (native + emscripten) ----
  [[nodiscard]] int getMidiInputCount() const noexcept
  {
    return wasm::midi_input_count<T>();
  }
  [[nodiscard]] int getMidiOutputCount() const noexcept
  {
    return wasm::midi_output_count<T>();
  }
  [[nodiscard]] int getMessageCount() const noexcept
  {
    return wasm::message_count<T>();
  }
  [[nodiscard]] int getCallbackCount() const noexcept
  {
    return wasm::callback_count<T>();
  }

  // ---- UI surface (native + emscripten) ----
  using ui_bridge_t = wasm::ui_bridge<T>;

  // True when the plug-in declares at least one custom paint item in its `ui`.
  [[nodiscard]] bool hasCustomUI() const noexcept
  {
    return ui_bridge_t::has_custom();
  }

  // Number of custom paint items (0 when none / no ui).
  [[nodiscard]] int getCustomUICount() const noexcept
  {
    return ui_bridge_t::custom_count();
  }

#if defined(__EMSCRIPTEN__)
  // --- MIDI ---
  // Push one event {bytes:Array<int>, timestamp:int} into midi input #portIndex.
  void pushMidi(int portIndex, emscripten::val event)
  {
    if(!m_block_open)
    {
      m_impl.begin_block();
      m_block_open = true;
    }
    wasm::push_midi_input<T>(m_impl.object, m_impl.midi, portIndex, event);
  }

  // Read every midi output port into {port, bytes, timestamp}. Call after process().
  emscripten::val drainMidi() { return wasm::drain_midi_outputs<T>(m_impl.object); }

  // --- Messages ---
  // Descriptor for message #i: {name, argCount}. argCount excludes the implicit T&.
  emscripten::val getMessageInfo(int i)
  {
    emscripten::val out = emscripten::val::null();
    if(i < 0 || i >= wasm::message_count<T>())
      return out;
    wasm::for_message_nth<T>(m_impl.object, i, [&]<typename M>(M& /*field*/) {
      out = emscripten::val::object();
      out.set("index", emscripten::val(i));
      out.set("name", emscripten::val(std::string{M::name()}));
      out.set("argCount", emscripten::val(wasm::message_js_arg_count<T, M>()));
    });
    return out;
  }

  // Invoke message `name` with argsVal (JS array). True if it matched and ran.
  bool sendMessage(std::string name, emscripten::val argsVal)
  {
    return wasm::dispatch_message<T>(m_impl.object, name, argsVal);
  }

  // --- Sample-accurate controls ---
  // Schedule a value into input control #paramIndex at sampleOffset in the next
  // block. Non-sample-accurate controls apply immediately (timestamp ignored).
  void scheduleControl(int paramIndex, int sampleOffset, emscripten::val value)
  {
    if(!m_block_open)
    {
      m_impl.begin_block();
      m_block_open = true;
    }
    wasm::schedule_control<T>(
        m_impl.object, m_impl.controls, paramIndex, sampleOffset, value,
        m_impl.m_buffer_size);
  }

  // Read output control ports back to JS after process():
  // array of {index, changes:[{frame,value}...], value}.
  emscripten::val getControlOutputs()
  {
    return wasm::drain_control_outputs<T>(m_impl.object, m_impl.controls);
  }

  // --- Callbacks (plug-in -> JS) ---
  // Descriptor for callback #i (0-based among callbacks): {index, name, argCount}.
  emscripten::val getCallbackInfo(int i)
  {
    emscripten::val out = emscripten::val::null();
    if(i < 0 || i >= wasm::callback_count<T>())
      return out;
    wasm::for_callback_nth<T>(m_impl.object, i, [&]<typename F>(F& /*field*/) {
      out = emscripten::val::object();
      out.set("index", emscripten::val(i));
      out.set("name", emscripten::val(std::string{avnd::get_name<F>()}));
      out.set("argCount", emscripten::val(wasm::callback_js_arg_count<F>()));
    });
    return out;
  }

  // Drain callbacks emitted since the last drain into an array of
  // {index, name, args:[...]}, then clear the queue.
  emscripten::val drainCallbacks()
  {
    if constexpr(wasm::callback_count<T>() > 0)
    {
      auto names = wasm::callback_names<T>(m_impl.object);
      return m_impl.callback_events.template drain<T>(names);
    }
    else
    {
      return emscripten::val::array();
    }
  }

  // --- UI ---
  // Serialize the plug-in's `ui` layout tree to a JS object (schema in ui_bridge.hpp).
  // Falls back to a flat vbox-of-controls when there is no `ui` member.
  emscripten::val getUILayout()
  {
    return ui_bridge_t::layout(m_impl.object);
  }

  // Draw the itemIndex-th custom paint item onto a CanvasRenderingContext2D.
  // No-op when the plug-in has no custom UI or itemIndex is out of range.
  void paintCustom(int itemIndex, emscripten::val ctx2d)
  {
    ui_bridge_t::paint(m_impl.object, itemIndex, ctx2d);
  }
#endif

protected:
  audio_processor<T> m_impl;
  bool m_block_open{false};
};

}

#if defined(__EMSCRIPTEN__)
// Registers the Embind class for plug-in `Type` under JS class name `Name`.
#define AVND_WASM_MODULE(Type, Name)                                          \
  EMSCRIPTEN_BINDINGS(Name##_module)                                          \
  {                                                                           \
    using module_t = wasm::plugin_module<Type>;                              \
    emscripten::class_<module_t>(#Name)                                       \
        .constructor<>()                                                      \
        .function("prepare", &module_t::prepare)                              \
        .function("process", &module_t::process)                              \
        .function("beginBlock", &module_t::beginBlock)                         \
        .function("inputChannels", &module_t::inputChannels)                  \
        .function("outputChannels", &module_t::outputChannels)                \
        .function("getMidiInputCount", &module_t::getMidiInputCount)          \
        .function("getMidiOutputCount", &module_t::getMidiOutputCount)        \
        .function("pushMidi", &module_t::pushMidi)                             \
        .function("drainMidi", &module_t::drainMidi)                           \
        .function("getMessageCount", &module_t::getMessageCount)              \
        .function("getMessageInfo", &module_t::getMessageInfo)                \
        .function("sendMessage", &module_t::sendMessage)                       \
        .function("getCallbackCount", &module_t::getCallbackCount)            \
        .function("getCallbackInfo", &module_t::getCallbackInfo)              \
        .function("drainCallbacks", &module_t::drainCallbacks)                \
        .function("scheduleControl", &module_t::scheduleControl)              \
        .function("getControlOutputs", &module_t::getControlOutputs)          \
        .function("getParameterCount", &module_t::getParameterCount)          \
        .function("getParameterInfo", &module_t::getParameterInfo)            \
        .function("setParameter", &module_t::setParameter)                    \
        .function("getParameter", &module_t::getParameter)                    \
        .function(                                                            \
            "setParameterNormalized", &module_t::setParameterNormalized)      \
        .function(                                                            \
            "getParameterNormalized", &module_t::getParameterNormalized)      \
        .function("getOutputCount", &module_t::getOutputCount)                \
        .function("getOutputInfo", &module_t::getOutputInfo)                  \
        .function("getOutputValue", &module_t::getOutputValue)                \
        .function("hasCustomUI", &module_t::hasCustomUI)                       \
        .function("getCustomUICount", &module_t::getCustomUICount)            \
        .function("getUILayout", &module_t::getUILayout)                       \
        .function("paintCustom", &module_t::paintCustom)                       \
        .class_function("getName", &module_t::name)                           \
        .class_function("getCName", &module_t::c_name)                         \
        .class_function("getUuid", &module_t::uuid)                            \
        .class_function("getAuthor", &module_t::author)                        \
        .class_function("getVersion", &module_t::version);                     \
  }
#else
// Native compile: force instantiation so the headers are type-checked by clang.
#define AVND_WASM_MODULE(Type, Name)                                          \
  template class wasm::plugin_module<Type>;
#endif
