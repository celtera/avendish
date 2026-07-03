#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Emscripten shell for the soft UI runtime: the framebuffer path of
 * CUSTOM_UI_PLAN.md §6.3, as an alternative to the DOM/Canvas2D UI of
 * binding/wasm/ui_bridge.hpp. The C++ side renders into its RGBA
 * framebuffer; JS blits it into a <canvas> with putImageData and feeds
 * pointer events back (see binding/wasm/js/avnd-soft-ui.js).
 *
 * v1 scope: a self-contained browser preview -- the shell owns its own
 * effect instance and the bus is wired synchronously by the runtime, like
 * the native standalone preview. Hooking it to the AudioWorklet processing
 * graph comes with emsdk validation.
 *
 * Validated headlessly under node (tests/ui/wasm_soft_ui_test.mjs: frame
 * determinism + pointer interaction through embind); the browser-side
 * canvas glue in avnd-soft-ui.js still awaits a manual browser run.
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

  // One frame: widget pass + rasterization. Returns a zero-copy
  // Uint8ClampedArray view over the RGBA framebuffer (valid until the next
  // resize) for `new ImageData(view, w, h)` + putImageData.
  emscripten::val frame()
  {
    const auto fb = m_surface.frame();
    return emscripten::val(emscripten::typed_memory_view(
        size_t(fb.width) * fb.height * 4,
        reinterpret_cast<const unsigned char*>(fb.data)));
  }

private:
  avnd::effect_container<T> m_effect;
  surface<T> m_surface;
};

template <typename T>
void bind_wasm_ui(const char* js_name)
{
  emscripten::class_<wasm_ui<T>>(js_name)
      .template constructor<>()
      .function("resize", &wasm_ui<T>::resize)
      .function("logicalWidth", &wasm_ui<T>::logical_width)
      .function("logicalHeight", &wasm_ui<T>::logical_height)
      .function("physicalWidth", &wasm_ui<T>::physical_width)
      .function("physicalHeight", &wasm_ui<T>::physical_height)
      .function("pointerMove", &wasm_ui<T>::pointer_move)
      .function("pointerButton", &wasm_ui<T>::pointer_button)
      .function("wheel", &wasm_ui<T>::wheel)
      .function("frame", &wasm_ui<T>::frame);
}
}

#endif
