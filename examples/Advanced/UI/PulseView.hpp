#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <smallfun.hpp>

#include <chrono>

namespace uo
{
struct PulseItem
{
  static constexpr double width() { return 10.; }
  static constexpr double height() { return 10.; }

  void paint(auto ctx)
  {
    if(ratio <= 0.)
      return;

    constexpr double side = 10.;
    ctx.begin_path();
    auto col = ctx.to_rgba(halp::colors::light);
    ctx.set_stroke_color(col);
    col.a *= ratio;
    ctx.set_fill_color(col);
    ctx.draw_rect(0, 0, side, side);
    ctx.fill();
    ctx.update();

    ratio = std::max(0.0, ratio - 0.25 / 240.0);
  }

  double ratio = 0.0;
  std::chrono::steady_clock::time_point m_last;
  smallfun::function<void()> update;
};

struct PulseView
{
  halp_meta(name, "Pulse View")
  halp_meta(c_name, "pulse_view")
  halp_meta(category, "Monitoring")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/pulse-view.html")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Pulses on incoming messages")
  halp_meta(uuid, "67bab0b4-0141-4474-bb11-57f2751a76ad")
  halp_flag(fully_custom_item);

  struct inputs
  {
    struct
    {
      halp_meta(name, "Input")
      enum widget
      {
        control
      };
      std::optional<halp::impulse> value;

      void update(PulseView& self)
      {
        if(value)
          self.send_message();
      }
    } in;
  };

  std::function<void()> send_message;

  struct ui
  {
    halp_meta(name, "Main")
    halp_meta(layout, halp::layouts::container)

    halp::custom_control<PulseItem, &inputs::in> anim{.x = 10, .y = 0};
    struct bus
    {
      static void process_message(ui& self)
      {
        self.anim.ratio = 1.0;
        self.anim.update();
      }
    };
  };
};
}
