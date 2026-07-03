#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Windowless shell around soft_ui::ui_runtime: owns the pixel storage and
 * exposes a plain "push events, pull pixels" API. This is the entry point
 * for WASM (blit via putImageData), microcontrollers (flush to LCD) and
 * headless tests; desktop plug-in editors use the pugl-based window shell
 * instead.
 */

#include <avnd/binding/ui/soft/runtime.hpp>

namespace avnd::soft_ui
{
template <typename T>
class surface
{
public:
  explicit surface(avnd::effect_container<T>& impl, font_registry fonts = {})
      : m_runtime{impl, std::move(fonts)}
  {
    resize(m_runtime.width(), m_runtime.height());
  }

  ui_runtime<T>& runtime() noexcept { return m_runtime; }

  void resize(int w, int h)
  {
    m_pixels.assign(size_t(w) * h * 4, 0);
    m_fb = framebuffer{m_pixels.data(), w, h, w * 4};
    m_runtime.set_viewport(w, h);
  }

  // ---- Input ----
  void pointer_move(double x, double y) { m_runtime.pointer_move(x, y); }
  void pointer_button(double x, double y, bool pressed)
  {
    m_runtime.pointer_button(x, y, pressed);
  }
  void wheel(double x, double y, double delta) { m_runtime.wheel(x, y, delta); }

  // ---- Frame: run one widget pass and rasterize. Returns the pixels. ----
  framebuffer frame()
  {
    m_runtime.tick();
    m_runtime.render(m_fb);
    return m_fb;
  }

  framebuffer pixels() noexcept { return m_fb; }

private:
  ui_runtime<T> m_runtime;
  std::vector<unsigned char> m_pixels;
  framebuffer m_fb{};
};
}
