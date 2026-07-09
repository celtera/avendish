#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Tier C custom-UI example (CUSTOM_UI_PLAN.md §5): instead of the
 * declarative `struct ui` rendered by the reference soft editor, the plug-in
 * ships its own editor implementing the avnd::gui_windowed_ui shape:
 *
 *     struct ui {
 *       struct window {
 *         void open(avnd::gui_parent, avnd::gui_host);
 *         void close();
 *         void idle();
 *         std::pair<int, int> size() const;
 *       };
 *     };
 *
 * Any UI stack works behind this seam -- Dear ImGui, Qt, JUCE, ... This
 * example deliberately uses no library at all: two hand-rolled implementations
 * of the same contract, a raw Win32 child window painted with GDI and a raw
 * Xlib window. So the contract itself is the whole story:
 *
 *  - open() receives the host's parent window handle and creates whatever
 *    the framework needs inside it;
 *  - parameter *reads* poll the effect model (live processor instance in
 *    CLAP/VST2, controller-side model in VST3), so host automation shows up
 *    without any extra plumbing;
 *  - parameter *writes* set the field, then drive the automation-gesture
 *    hooks on avnd::gui_host (flat parameter index, [0; 1] normalized) --
 *    the bindings translate to CLAP gesture events / audioMasterAutomate /
 *    IComponentHandler;
 *  - ticks come from the host (clap.timer-support, effEditIdle) and from
 *    the host's message pump for frameworks with their own timers.
 *
 * The two platform implementations live in the sibling translation units
 * CustomUiWindow.win32.cpp and CustomUiWindow.x11.cpp, reached through a
 * forward-declared pimpl so this header pulls in no windowing headers at all
 * (a real toolkit would hide the same way behind its own opaque handle). On
 * platforms with neither backend the plug-in simply has no editor: the
 * concept is not satisfied and the bindings skip it.
 */

#include <avnd/concepts/gui_window.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <functional>
#include <memory>
#include <utility>

namespace examples
{
struct CustomUiWindow
{
  halp_meta(name, "Custom UI window")
  halp_meta(c_name, "avnd_custom_ui_window")
  halp_meta(category, "Demo")
  halp_meta(author, "Avendish")
  halp_meta(description, "Gain with a hand-rolled (raw Win32 / raw Xlib) editor")
  halp_meta(uuid, "aef14f10-fb50-45e9-9dc6-c40c5e5fe790")

  struct ins
  {
    halp::knob_f32<"Gain", halp::range{.min = 0., .max = 1., .init = 0.5}> gain;
    halp::dynamic_audio_bus<"In", float> audio;
  } inputs;

  struct outs
  {
    halp::dynamic_audio_bus<"Out", float> audio;
  } outputs;

  void operator()(int frames)
  {
    for(int c = 0; c < outputs.audio.channels; c++)
      for(int i = 0; i < frames; i++)
        outputs.audio.samples[c][i] = inputs.audio.samples[c][i] * inputs.gain.value;
  }

  // Message bus for Tier C: the window itself is the UI-side endpoint --
  // the bindings wire window.send_message and call window.process_message.
  struct ui_to_processor
  {
    float value;
  };
  struct processor_to_ui
  {
    float applied;
  };

  void process_message(const ui_to_processor& msg)
  {
    // Deliberately not the same as the direct edit (msg.value / 2) so the
    // bus path is observable in tests.
    inputs.gain.value = msg.value * 0.5f;
    if(send_message)
      send_message(processor_to_ui{.applied = inputs.gain.value});
  }
  std::function<void(processor_to_ui)> send_message;

// The editor exists only where a windowing implementation backs it: raw Win32
// on Windows, raw Xlib on Linux when the X11 headers are present. Both are
// defined out-of-line in CustomUiWindow.{win32,x11}.cpp behind the pimpl below.
#if defined(_WIN32) || (defined(__linux__) && __has_include(<X11/Xlib.h>))
  struct ui
  {
    struct window
    {
      static constexpr int width = 360, height = 120;
      static constexpr int gain_param = 0; // flat index in parameter order

      // The bindings pass the model when the constructor accepts it
      // (avnd::make_ui_editor); in CLAP/VST2 this is the live processor
      // instance, in VST3 the controller-side model.
      explicit window(avnd::effect_container<CustomUiWindow>& fx);
      ~window();

      // avnd::gui_windowed_ui contract, forwarded to the platform impl.
      void open(avnd::gui_parent parent, avnd::gui_host h);
      void close();
      void idle();
      std::pair<int, int> size() const { return {width, height}; }

      // ---- Message bus endpoints (wired by the bindings) ----
      // UI thread -> processor; the impl calls it on mouse release.
      std::function<void(ui_to_processor)> send_message;
      // processor -> UI thread
      void process_message(processor_to_ui msg) { feedback = msg.applied; }
      float feedback{-1.f};

    private:
      // Platform window + its paint/event code, defined per-OS in the matching
      // .cpp; forward-declared here so no windowing header reaches this file.
      struct impl;
      std::unique_ptr<impl> self;
    };
  };
#endif
};
}
