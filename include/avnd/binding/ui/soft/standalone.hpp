#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Standalone UI preview: opens a plug-in's editor (declarative layout,
 * custom paint() widgets, or an author-provided Tier C window that supports
 * top-level use) in its own window, without any plug-in host. Made for
 * `avnd_make_ui_preview` targets so every example UI can be run and
 * eyeballed directly.
 *
 * The bus is wired synchronously by the soft runtime (single thread, no
 * host): messages round-trip within the process.
 */

#include <avnd/binding/ui/editor.hpp>

#include <chrono>
#include <thread>

namespace avnd::soft_ui
{
// Runs until the window is closed; max_frames >= 0 exits after that many
// update cycles (used by smoke tests).
template <typename T>
  requires avnd::has_ui_editor<T>
int run_editor(int max_frames = -1)
{
  avnd::effect_container<T> effect;
  if constexpr(avnd::has_inputs<T>)
    avnd::init_controls(effect);

  auto editor = avnd::make_ui_editor(effect);

  bool quit = false;
  if constexpr(requires { editor->on_close; })
    editor->on_close = [&quit] { quit = true; };

  avnd::gui_parent parent{.handle = nullptr, .scale = 1.};
#if defined(_WIN32)
  parent.api = avnd::gui_api::win32_hwnd;
#elif defined(__APPLE__)
  parent.api = avnd::gui_api::cocoa_nsview;
#else
  parent.api = avnd::gui_api::x11_window;
#endif
  editor->open(parent, avnd::gui_host{});

  for(int frame = 0; !quit && (max_frames < 0 || frame < max_frames); ++frame)
  {
    if constexpr(requires { editor->update(0.); })
    {
      editor->update(1. / 60.);
    }
    else
    {
      editor->idle();
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
  }

  editor->close();
  return 0;
}
}
