#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Small gain plug-in with a declarative UI + interactive custom widget,
// built as a .clap by the test suite to exercise the clap.gui glue
// end-to-end (see test_clap_gui.cpp).

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/custom_widgets.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace avnd_test
{
struct clap_ui_level_widget
{
  static constexpr double width() { return 200.; }
  static constexpr double height() { return 100.; }

  void paint(auto ctx)
  {
    ctx.set_fill_color({30, 30, 30, 255});
    ctx.begin_path();
    ctx.draw_rect(0., 0., width(), height());
    ctx.fill();

    ctx.set_fill_color({255, 176, 30, 255});
    ctx.begin_path();
    ctx.draw_rect(0., 0., value * width(), height());
    ctx.fill();
  }

  bool mouse_press(double x, double y)
  {
    transaction.start();
    update_from(x);
    transaction.update(value);
    return true;
  }
  bool mouse_move(double x, double y)
  {
    update_from(x);
    transaction.update(value);
    return true;
  }
  bool mouse_release(double x, double y)
  {
    transaction.commit();
    if(on_commit)
      on_commit(value);
    return false;
  }

  void update_from(double x)
  {
    value = float(std::clamp(x / width(), 0., 1.));
  }

  static float value_to_control(auto& control, float v) { return v; }
  void set_value(const auto& control, float v) { value = v; }

  halp::transaction<float> transaction;
  std::function<void(float)> on_commit;
  float value{};
  float feedback{};
};

struct ClapUiPlug
{
  halp_meta(name, "Clap UI test")
  halp_meta(c_name, "avnd_clap_ui_test")
  halp_meta(uuid, "9b1e5a76-52a3-4e2f-8c07-3d3b7e0a5f42")

  struct ins
  {
    // Parameters first so their flat indices stay 0 (Level) and 1 (Gain)
    struct : halp::val_port<"Level", float>
    {
      struct range
      {
        float min = 0., max = 1., init = 0.25;
      };
    } level;
    halp::knob_f32<"Gain", halp::range{.min = 0., .max = 1., .init = 0.5}> gain;

    halp::dynamic_audio_bus<"In", float> audio;
  } inputs;

  struct outs
  {
    halp::dynamic_audio_bus<"Out", float> audio;
  } outputs;

  void operator()(int frames)
  {
    const float g = inputs.gain.value * inputs.level.value;
    for(int c = 0; c < outputs.audio.channels; c++)
    {
      auto* in = inputs.audio.samples[c];
      auto* out = outputs.audio.samples[c];
      for(int i = 0; i < frames; i++)
        out[i] = in[i] * g;
    }
  }

  // Message bus round trip: committed widget values are sent to the
  // processor (drained in process()), which copies them into the gain
  // parameter (host-observable) and answers with a feedback message
  // (drained on the editor timer).
  //
  // The non-trivial members (string, vector) exercise the bus serializer on
  // bindings where the payload crosses a host boundary (VST3 IMessage): the
  // processor only applies the value when they arrive intact, so the
  // host-observable gain assertion doubles as a serialization check.
  struct ui_to_processor
  {
    float value;
    std::string label;
    std::vector<float> curve;
  };
  struct processor_to_ui
  {
    float doubled;
    std::string echoed;
  };

  void process_message(const ui_to_processor& msg)
  {
    if(msg.label != "commit" || msg.curve != std::vector<float>{1.f, 2.f, 3.f})
      return;
    inputs.gain.value = msg.value;
    if(send_message)
      send_message(processor_to_ui{.doubled = msg.value * 2.f, .echoed = "ok"});
  }
  std::function<void(processor_to_ui)> send_message;

  struct ui
  {
    halp_meta(layout, halp::layouts::container)
    halp_meta(width, 320)
    halp_meta(height, 220)

    halp::custom_control<clap_ui_level_widget, &ins::level> level{};

    struct bus
    {
      void init(ui& self)
      {
        self.level.on_commit = [this](float v) {
          this->send_message(
              ui_to_processor{.value = v, .label = "commit", .curve = {1.f, 2.f, 3.f}});
        };
      }
      static void process_message(ui& self, processor_to_ui msg)
      {
        if(msg.echoed == "ok")
          self.level.feedback = msg.doubled;
      }
      std::function<void(ui_to_processor)> send_message;
    };
  };
};
}
