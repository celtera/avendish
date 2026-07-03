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
    return false;
  }

  void update_from(double x)
  {
    value = float(std::clamp(x / width(), 0., 1.));
  }

  static float value_to_control(auto& control, float v) { return v; }
  void set_value(const auto& control, float v) { value = v; }

  halp::transaction<float> transaction;
  float value{};
};

struct ClapUiPlug
{
  halp_meta(name, "Clap UI test")
  halp_meta(c_name, "avnd_clap_ui_test")
  halp_meta(uuid, "9b1e5a76-52a3-4e2f-8c07-3d3b7e0a5f42")

  struct ins
  {
    struct : halp::val_port<"Level", float>
    {
      struct range
      {
        float min = 0., max = 1., init = 0.25;
      };
    } level;
    halp::knob_f32<"Gain", halp::range{.min = 0., .max = 1., .init = 0.5}> gain;
  } inputs;

  struct
  {
    halp::audio_channel<"In", float> audio;
  } input_audio;

  struct
  {
    halp::audio_channel<"Out", float> audio;
  } output_audio;

  void operator()(int frames)
  {
    for(int i = 0; i < frames; i++)
      output_audio.audio.channel[i]
          = input_audio.audio.channel[i] * inputs.gain.value * inputs.level.value;
  }

  struct ui
  {
    halp_meta(layout, halp::layouts::container)
    halp_meta(width, 320)
    halp_meta(height, 220)

    halp::custom_control<clap_ui_level_widget, &ins::level> level{};
  };
};
}
