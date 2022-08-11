#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/painter.hpp>
#include <avnd/concepts/processor.hpp>
#include <avnd/wrappers/controls.hpp>
#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/custom_widgets.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>

#include <cstdio>

namespace examples::helpers
{
struct custom_slider
{
  static constexpr double width() { return 100.; }
  static constexpr double height() { return 20.; }

  void set_value(const auto& control, double value)
  {
    this->value = avnd::map_control_to_01(control, value);
  }

  static auto value_to_control(auto& control, double value)
  {
    return avnd::map_control_from_01(control, value);
  }

  void paint(avnd::painter auto ctx)
  {
    ctx.set_stroke_color({200, 200, 200, 255});
    ctx.set_stroke_width(2.);
    ctx.set_fill_color({120, 120, 120, 255});
    ctx.begin_path();
    ctx.draw_rect(0., 0., width(), height());
    ctx.fill();
    ctx.stroke();

    ctx.begin_path();
    ctx.set_fill_color({90, 90, 90, 255});
    ctx.draw_rect(2., 2., (width() - 4) * value, (height() - 4));
    ctx.fill();
  }

  bool mouse_press(double x, double y)
  {
    transaction.start();
    mouse_move(x, y);
    return true;
  }

  void mouse_move(double x, double y)
  {
    const double res = std::clamp(x / width(), 0., 1.);
    transaction.update(res);
  }

  void mouse_release(double x, double y)
  {
    mouse_move(x, y);
    transaction.commit();
  }

  halp::transaction<double> transaction;
  double value{};
};

struct custom_anim
{
  using item_type = custom_anim;
  static constexpr double width() { return 200.; }
  static constexpr double height() { return 200.; }

  void paint(avnd::painter auto ctx)
  {
    constexpr double side = 40.;

    ctx.translate(100, 100);
    ctx.rotate(rot += 0.1);
    for(int i = 0; i < 10; i++)
    {
      ctx.translate(10, 10);
      ctx.rotate(5. + 0.1 * rot);
      ctx.scale(0.8, 0.8);
      ctx.begin_path();

      ctx.set_stroke_color({.r = 92, .g = 53, .b = 102, .a = 255});
      ctx.set_fill_color({173, 127, 168, 255});
      ctx.draw_rect(-side / 2., -side / 2., side, side);
      ctx.fill();
      ctx.stroke();
    }

    ctx.update();
  }

  double rot{};
};

/**
 * Example to test UI.
 */

struct AdvancedUi
{
  static consteval auto name() { return "ImageUI example"; }
  static consteval auto c_name() { return "avnd_image_ui"; }
  static consteval auto uuid() { return "001a50e1-828e-4f10-973c-bf750f6c6fc7"; }

  struct ins
  {
    halp::knob_f32<"Float", halp::range{.min = -1000., .max = 1000., .init = 100.}>
        float_ctl;
  } inputs;

  struct
  {
  } outputs;

  void operator()(int N) { }

  struct ui
  {
    halp_meta(name, "Main")
    halp_meta(layout, halp::layouts::container)
    halp_meta(background, "background.svg")
    halp_meta(width, 400)
    halp_meta(height, 200)
    halp_meta(font, "Inconsolata")

    halp::custom_item<custom_slider, &ins::float_ctl> widget{{.x = 190, .y = 170}};
    halp::custom_item_base<custom_anim> anim{.x = 90, .y = -50};
  };
};
}
