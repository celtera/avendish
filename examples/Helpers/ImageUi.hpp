#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/processor.hpp>
#include <avnd/concepts/painter.hpp>
#include <avnd/wrappers/controls.hpp>
#include <halp/custom_widgets.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <cmath>

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
    static constexpr double width() { return 1000.; }
    static constexpr double height() { return 1000.; }

    void paint(avnd::painter auto ctx)
    {
      constexpr double side = 40.;


        //White Square --> ok
//        ctx.set_fill_color({255, 255, 255, 255});
//        ctx.draw_rect(50, 50, 50, 50);
//        ctx.fill();

        //White Circle --> ok
//        ctx.set_fill_color({255, 255, 255, 255});
//        ctx.draw_circle(50, 100, 50);
//        ctx.fill();

        //Text --> ok
//        ctx.set_fill_color({255, 255, 255, 255});
//        ctx.set_font("Ubuntu");
//        ctx.set_font_size(15);
//        ctx.draw_text(50,100, "Hello World !");
//        ctx.fill();

        //Image --> ok
//        ctx.begin_path();
//        ctx.draw_pixmap(50,50,"//home/scrime/Images/StageScrime/external-content.duckduckgo.com.jpeg");

        //Triangle --> ok
//        ctx.set_fill_color({0, 0, 204, 255});
//        ctx.draw_triangle(250, 500, 750, 500, 500, 250);
//        ctx.fill();

        //Gradient --> ok
//        ctx.set_fill_color({255, 255, 255, 255});
//        ctx.set_linear_gradient(250, 250, 250, 750, {0, 255, 234, 255}, {239, 0, 255, 255});
//        //ctx.set_radial_gradient(500, 500, 250, {0, 255, 234, 255}, {239, 0, 255, 255});
//        //ctx.set_conical_gradient(500, 500, 90, {0, 255, 234, 255}, {239, 0, 255, 255});
//        ctx.draw_rect(250, 250, 500, 500);
//        ctx.fill();

        //Polygon --> ok
//        double tab[8] = {250, 500, 750, 500, 500, 250, 0, 0};
//        ctx.set_fill_color({0, 0, 204, 255});
//        ctx.draw_polygon(tab, 4);
//        ctx.fill();

        //Star --> ok
//        ctx.set_fill_color({200, 0, 0, 255});
//        ctx.set_linear_gradient(0, 0, 1000, 1000, {0, 255, 234, 255}, {239, 0, 255, 255});
//        ctx.draw_star(500, 500, 100, 400, 25);
//        ctx.fill();

        //Clock --> test
        ctx.draw_analog_clock(width(), height(), {127, 0, 127, 255}, {0, 127, 127, 190}, {0, 0, 0, 255});

        return;


      ctx.translate(100, 100);
      ctx.rotate(rot += 0.1);
      for(int i = 0; i < 10; i++)
      {
      ctx.translate(10, 10);
      ctx.rotate(5.+ 0.1 * rot);
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
    halp::knob_f32<"Float", halp::range{.min = -1000., .max = 1000., .init = 100.}> float_ctl;
    //halp::knob_f32<"Test", halp::range{.min = 0., .max = 1., .init = 0.5}> tests;
  } inputs;

  struct { } outputs;

  void operator()(int N) { }

  struct ui {
      halp_meta(name, "Main")
      halp_meta(layout, halp::layouts::container)
      halp_meta(background, "background.svg")
      halp_meta(width, 400)
      halp_meta(height, 200)
      halp_meta(font, "Inconsolata")

      halp::custom_item<custom_slider, &ins::float_ctl> widget{{.x = 500, .y = 920}};
      halp::custom_item_base<custom_anim> anim{.x = 90, .y = -50};
  };
};
}
