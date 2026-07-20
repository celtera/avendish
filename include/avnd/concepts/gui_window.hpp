#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <cstdint>
#include <utility>

namespace avnd
{
// Host-attachment layer for plug-in editors (VST2 effEditOpen, VST3 IPlugView,
// CLAP clap.gui, standalone windows, ...). The binding owns the host protocol;
// the UI only ever sees these three types. See CUSTOM_UI_PLAN.md.

enum class gui_api : uint8_t
{
  win32_hwnd,
  cocoa_nsview,
  x11_window,
  wayland_surface,
  web_canvas,
  framebuffer
};

// What the host / binding hands the UI when opening it.
struct gui_parent
{
  // HWND / NSView* / (uintptr_t)X11 Window / CSS selector; null for
  // gui_api::framebuffer where the shell only ever asks for pixels.
  void* handle{};
  gui_api api{};
  // Host-reported HiDPI factor. 1.0 when the host has no scale protocol.
  double scale{1.};
};

// Callbacks the UI may invoke on the binding. UI thread only.
// Parameter indices are flat indices in avnd::parameter_input_introspection
// order; values are normalized to [0, 1]. Only the automation *gesture*
// protocol lives here -- parameter values travel through each binding's
// parameter mirror, bus messages through its message-bus transport.
struct gui_host
{
  void (*begin_edit)(void* ctx, int param) = [](void*, int) {};
  void (*perform_edit)(void* ctx, int param, double normalized)
      = [](void*, int, double) {};
  void (*end_edit)(void* ctx, int param) = [](void*, int) {};
  void (*request_resize)(void* ctx, int w, int h) = [](void*, int, int) {};
  void* ctx{};
};

// The escape hatch: T::ui::window with this shape can be attached by any
// plug-in binding, whatever framework it is implemented with. The reference
// soft:: runtime is itself exposed through this concept.
template <typename W>
concept gui_windowed_ui = requires(W w, gui_parent p, gui_host h) {
  w.open(p, h);
  w.close();
  w.idle();
  { w.size() } -> std::convertible_to<std::pair<int, int>>;
};

// Optional refinements, detected individually by bindings:
//   w.set_scale(2.0);            -- host DPI changed
//   w.set_size(800, 600);        -- host resized the editor
//   { W::resizable() } -> bool;
//   { w.supports(gui_api{}) } -> bool;
template <typename W>
concept gui_resizable_ui = gui_windowed_ui<W> && requires(W w) {
  w.set_size(0, 0);
  { W::resizable() } -> std::convertible_to<bool>;
};

// Detect T::ui::window on a processor type.
template <typename T>
concept has_gui_window = requires { sizeof(typename T::ui::window); }
                         && gui_windowed_ui<typename T::ui::window>;
}
