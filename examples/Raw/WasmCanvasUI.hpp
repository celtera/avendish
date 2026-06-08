#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Gain processor exercising the WASM Canvas2D UI backend: custom paint items
// plus a structured layout.

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>

#include <algorithm>

namespace examples
{
struct WasmCanvasUI
{
  halp_meta(name, "Wasm Canvas UI")
  halp_meta(c_name, "wasm_canvas_ui")
  halp_meta(category, "Demo")
  halp_meta(author, "Avendish")
  halp_meta(description, "Gain with a custom Canvas2D meter UI")
  halp_meta(uuid, "0d6f6a64-1c2e-4f7a-9b1a-2f4d6e8c0a11")

  struct ins
  {
    halp::hslider_f32<"Gain", halp::range{0., 1., 0.5}> gain;
    halp::toggle<"Bypass"> bypass;
  } inputs;

  struct
  {
    halp::audio_channel<"In", double> audio;
  } input_audio;

  struct
  {
    halp::audio_channel<"Out", double> audio;
  } output_audio;

  void operator()(int frames)
  {
    const double g = inputs.bypass.value ? 1.0 : inputs.gain.value;
    auto& in = input_audio.audio;
    auto& out = output_audio.audio;
    for(int i = 0; i < frames; ++i)
      out.channel[i] = in.channel[i] * g;
  }

  // Custom paint item: meter bar whose width tracks `gain`.
  struct MeterItem
  {
    static constexpr double width() { return 200.; }
    static constexpr double height() { return 24.; }

    double gain = 0.0;
    bool bypass = false;

    // WASM sync hook: called just before each paint().
    void update_from(const ins& in)
    {
      gain = in.gain.value;
      bypass = in.bypass.value;
    }

    void paint(auto ctx)
    {
      constexpr double w = width();
      constexpr double h = height();

      ctx.begin_path();
      ctx.set_fill_color({.r = 20, .g = 20, .b = 24, .a = 255});
      ctx.draw_rect(0., 0., w, h);
      ctx.fill();

      const double g = std::clamp(gain, 0.0, 1.0);
      const double bar_w = g * w;
      ctx.begin_path();
      if(bypass)
        ctx.set_fill_color({.r = 200, .g = 80, .b = 80, .a = 255});
      else
        ctx.set_fill_color({.r = 60, .g = 200, .b = 120, .a = 255});
      ctx.draw_rect(0., 0., bar_w, h);
      ctx.fill();

      ctx.begin_path();
      ctx.set_stroke_color({.r = 255, .g = 255, .b = 255, .a = 255});
      ctx.set_stroke_width(1.);
      ctx.draw_rect(0., 0., w, h);
      ctx.stroke();

      ctx.update();
    }
  };

  // Exercises the rest of the painter primitives for the painter-mapping test.
  struct PrimitivesItem
  {
    static constexpr double width() { return 64.; }
    static constexpr double height() { return 64.; }

    void paint(auto ctx)
    {
      ctx.set_font("Arial");
      ctx.set_font_size(12.);
      ctx.draw_text(1., 10., "hi");

      ctx.translate(32., 32.);
      ctx.rotate(90.);
      ctx.scale(2., 2.);

      ctx.begin_path();
      ctx.draw_circle(0., 0., 10.);
      ctx.draw_ellipse(0., 0., 20., 10.);
      ctx.arc_to(0., 0., 20., 20., 0., 90.);
      ctx.stroke();

      ctx.draw_line(0., 0., 5., 5.);
      ctx.reset_transform();

      // 2x1 RGBA8 image.
      static const unsigned char px[8]
          = {255, 0, 0, 255, 0, 255, 0, 255};
      ctx.draw_bytes(0., 0., 2., 1., px, 2, 1);
    }
  };

  struct ui
  {
    halp_meta(name, "Main")
    halp_meta(layout, halp::layouts::vbox)

    struct
    {
      halp_meta(name, "Controls")
      halp_meta(layout, halp::layouts::group)

      halp::item<&ins::gain> gain_widget;
      halp::item<&ins::bypass> bypass_widget;
    } controls;

    halp::custom_actions_item<MeterItem> meter{.x = 0, .y = 0};
    halp::custom_actions_item<PrimitivesItem> prims{.x = 0, .y = 0};
  };
};
}
