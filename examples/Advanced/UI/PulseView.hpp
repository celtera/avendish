#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <smallfun.hpp>

#include <chrono>

namespace uo
{
// A dot in the runtime-value (accent) colour, lit when a message arrives and
// fading out over 300 ms of wall-clock time.
struct PulseItem
{
  static constexpr double width() { return 10.; }
  static constexpr double height() { return 10.; }
  static constexpr auto fade = std::chrono::milliseconds{300};

  void pulse()
  {
    m_last = std::chrono::steady_clock::now();
    m_lit = true;
  }

  void paint(auto ctx)
  {
    if(!m_lit)
      return;
    const auto elapsed = std::chrono::steady_clock::now() - m_last;
    const double level
        = 1. - std::chrono::duration<double>(elapsed) / std::chrono::duration<double>(fade);
    if(level <= 0.)
    {
      m_lit = false;
      return;
    }

    ctx.begin_path();
    auto col = ctx.to_rgba(halp::colors::runtime_value_mid);
    col.a = uint8_t(col.a * level);
    ctx.set_fill_color(col);
    ctx.draw_circle(5., 5., 3.);
    ctx.fill();
    // Repaint until it has faded
    ctx.update();
  }

  std::chrono::steady_clock::time_point m_last;
  bool m_lit{};
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
        self.anim.pulse();
        self.anim.update();
      }
    };
  };
};
}
