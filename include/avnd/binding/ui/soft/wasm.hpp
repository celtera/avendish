#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Emscripten shell for the soft UI runtime: the framebuffer path of
 * CUSTOM_UI_PLAN.md §6.3, as an alternative to the DOM/Canvas2D UI of
 * binding/wasm/ui_bridge.hpp. The C++ side renders into its RGBA
 * framebuffer; JS blits it into a <canvas> with putImageData and feeds
 * pointer events back (see binding/wasm/js/avnd-soft-ui.js).
 *
 * The shell paints its own effect instance (widget state), but the
 * authoritative instance is the page's audio worklet: gestures are
 * reported through setGestureCallback (the page forwards them as
 * setParamNorm worklet messages), and the page pushes worklet param /
 * output state back in via setParameter / setOutputValue. The ui::bus
 * stays wired synchronously to the local instance (worklet bus routing is
 * an open item).
 *
 * Usage in a module:
 *
 *   #include <avnd/binding/ui/soft/implementation.hpp> // once per module
 *   #include <avnd/binding/ui/soft/wasm.hpp>
 *   EMSCRIPTEN_BINDINGS(my_ui) { avnd::soft_ui::bind_wasm_ui<MyPlugin>("SoftUI"); }
 *
 * and from JS:
 *
 *   import { attachSoftUI } from "./avnd-soft-ui.js";
 *   attachSoftUI(new Module.SoftUI(), canvasElement);
 */

#if defined(__EMSCRIPTEN__)

#include <avnd/binding/ui/soft/surface.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

namespace avnd::soft_ui
{
template <typename T>
class wasm_ui
{
public:
  wasm_ui()
      : m_surface{m_effect, system_fonts()}
  {
    if constexpr(avnd::has_inputs<T>)
      avnd::init_controls(m_effect);
  }

  // devicePixelRatio-aware: logical CSS size + DPR
  void resize(int w, int h, double device_pixel_ratio)
  {
    m_surface.resize(w, h, device_pixel_ratio);
  }

  // ---- Shared-instance wiring -------------------------------------------
  // fn(type: "begin"|"perform"|"end", paramIndex: int, normalized: double).
  // Wires the runtime's gui_host so user edits on the canvas are reported
  // to JS with the same flat-index/normalized protocol as native hosts.
  void set_gesture_callback(emscripten::val fn)
  {
    m_on_gesture = std::move(fn);
    auto& h = m_surface.runtime().host;
    h.ctx = this;
    h.begin_edit = [](void* ctx, int p) {
      ((wasm_ui*)ctx)->fire_gesture("begin", p, 0.);
    };
    h.perform_edit = [](void* ctx, int p, double norm) {
      ((wasm_ui*)ctx)->fire_gesture("perform", p, norm);
    };
    h.end_edit
        = [](void* ctx, int p) { ((wasm_ui*)ctx)->fire_gesture("end", p, 0.); };
  }

  // External (DOM- or worklet-driven) parameter change: raw value, no
  // gesture feedback — mirrors what native bindings do for host automation.
  void set_parameter(int index, double v)
  {
    using pi = avnd::parameter_input_introspection<T>;
    if constexpr(pi::size > 0)
    {
      if(index < 0 || index >= (int)pi::size)
        return;
      for(auto state : m_effect.full_state())
      {
        pi::for_nth_mapped(state.inputs, index, [&]<typename C>(C& field) {
          if constexpr(requires { field.value = decltype(field.value)(v); })
            field.value = decltype(field.value)(v);
        });
      }
      m_surface.runtime().mark_dirty();
    }
  }

  // Worklet output values (bargraphs...): raw output-field index, the same
  // space as the page's {type:'outputs'} poll.
  void set_output_value(int index, double v)
  {
    using oi = avnd::output_introspection<T>;
    if constexpr(oi::size > 0)
    {
      if(index < 0 || index >= (int)oi::size)
        return;
      for(auto state : m_effect.full_state())
      {
        oi::for_nth(state.outputs, index, [&]<typename C>(C& field) {
          if constexpr(requires { field.value = decltype(field.value)(v); })
            field.value = decltype(field.value)(v);
        });
      }
      m_surface.runtime().mark_dirty();
    }
  }

  // Fonts cannot come from the filesystem in a browser (and --embed-file's
  // FS emulation breaks the module inside AudioWorkletGlobalScope): the
  // page fetches a TTF and passes it in as a Uint8Array.
  void load_font(emscripten::val bytes)
  {
    m_surface.runtime().fonts().register_font(
        "default",
        emscripten::convertJSArrayToNumberVector<unsigned char>(bytes));
    m_surface.runtime().mark_dirty();
  }

  // Declared logical size of the layout (before any resize)
  int logical_width() { return m_surface.runtime().width(); }
  int logical_height() { return m_surface.runtime().height(); }

  int physical_width() { return m_surface.runtime().physical_width(); }
  int physical_height() { return m_surface.runtime().physical_height(); }

  // Pointer coordinates in physical canvas pixels
  void pointer_move(double x, double y) { m_surface.pointer_move(x, y); }
  void pointer_button(double x, double y, bool pressed)
  {
    m_surface.pointer_button(x, y, pressed);
  }
  void wheel(double x, double y, double delta) { m_surface.wheel(x, y, delta); }

  // One frame: widget pass + rasterization when something changed. Returns
  // a zero-copy Uint8ClampedArray view over the RGBA framebuffer (valid
  // until the next resize) for `new ImageData(view, w, h)` + putImageData,
  // or null when the UI is clean — the caller keeps the previous canvas
  // contents and skips the blit (idle frames cost ~nothing).
  emscripten::val frame()
  {
    const auto fb = m_surface.frame_if_dirty();
    if(!fb.data)
      return emscripten::val::null();
    return emscripten::val(emscripten::typed_memory_view(
        size_t(fb.width) * fb.height * 4,
        reinterpret_cast<const unsigned char*>(fb.data)));
  }

  // Unconditional render (first paint, testing)
  emscripten::val render_now()
  {
    const auto fb = m_surface.frame();
    return emscripten::val(emscripten::typed_memory_view(
        size_t(fb.width) * fb.height * 4,
        reinterpret_cast<const unsigned char*>(fb.data)));
  }

private:
  void fire_gesture(const char* type, int param, double norm)
  {
    if(!m_on_gesture.isUndefined() && !m_on_gesture.isNull())
      m_on_gesture(std::string(type), param, norm);
  }

  avnd::effect_container<T> m_effect;
  surface<T> m_surface;
  emscripten::val m_on_gesture = emscripten::val::undefined();
};

template <typename T>
void bind_wasm_ui(const char* js_name)
{
  emscripten::class_<wasm_ui<T>>(js_name)
      .template constructor<>()
      .function("resize", &wasm_ui<T>::resize)
      .function("setGestureCallback", &wasm_ui<T>::set_gesture_callback)
      .function("setParameter", &wasm_ui<T>::set_parameter)
      .function("setOutputValue", &wasm_ui<T>::set_output_value)
      .function("loadFont", &wasm_ui<T>::load_font)
      .function("logicalWidth", &wasm_ui<T>::logical_width)
      .function("logicalHeight", &wasm_ui<T>::logical_height)
      .function("physicalWidth", &wasm_ui<T>::physical_width)
      .function("physicalHeight", &wasm_ui<T>::physical_height)
      .function("pointerMove", &wasm_ui<T>::pointer_move)
      .function("pointerButton", &wasm_ui<T>::pointer_button)
      .function("wheel", &wasm_ui<T>::wheel)
      .function("frame", &wasm_ui<T>::frame)
      .function("renderNow", &wasm_ui<T>::render_now);
}
}

#endif
