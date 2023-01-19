#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/painter.hpp>
#include <avnd/concepts/processor.hpp>
#include <avnd/wrappers/controls.hpp>
#include <cmath>
#include <halp/layout.hpp>

#include <cstdio>

namespace dspl
{
struct color_display
{
  static constexpr double width() { return 100.; }
  static constexpr double height() { return 100.; }

  void set_value(const auto& control, std::vector<float> value)
  {
    this->value = avnd::map_control_to_01(control, value);
  }

  static auto value_to_control(auto& control, std::vector<float> value)
  {
    return avnd::map_control_from_01(control, value);
  }

  void paint(avnd::painter auto ctx)
  {
    ctx.begin_path();
    // r,g,b,a
    ctx.set_fill_color({value[0], value[1], value[2], value[3]});
    ctx.draw_rect(2., 2., width(), height());
    ctx.fill();
  }

  std::function<void()> update;
  std::array<uint8_t, 4> value{};
};

struct ColorDisplay
{
  static consteval auto name() { return "Color display"; }
  static consteval auto c_name() { return "avnd_color_display"; }
  static consteval auto uuid() { return "4473f4fb-509c-4aff-8762-e32f383673ec"; }

  ossia::argb_u m_value0;

  ossia::rgba_u m_value1;

  ossia::rgb_u m_value2;

  ossia::bgr_u m_value3;

  ossia::argb8_u m_value4;

  ossia::rgba8_u m_value5;

  ossia::hsv_u m_value6;

  ossia::cmy8_u m_value7;

  ossia::xyz_u m_value8;

  struct ins
  {
    halp::val_port<"Color", std::array<float, 4>> float_ctl;
    struct
    {
      halp__enum("Format", RGBA, RGBA, RGBA8, ARGB, ARGB8, BGR, HSV, CMY);
    } format;
  } inputs;

  struct
  {
  } outputs;

  void operator()(int N)
  {
    auto v = inputs.float_ctl.value;
    using fmt = decltype(inputs.format)::enum_type;

    ossia::vec4f rgbaf;

    switch(inputs.format)
    {
      case fmt::RGBA:
        rgbaf = ossia::rgba8{ossia::rgba{v}}.dataspace_value;
        break;
      case fmt::RGBA8:
        rgbaf = ossia::rgba8{v}.dataspace_value;
        break;
      case fmt::ARGB:
        rgbaf = ossia::rgba8{ossia::argb{v}}.dataspace_value;
        break;
      case fmt::ARGB8:
        rgbaf = ossia::rgba8{ossia::argb8{v}}.dataspace_value;
        break;
      case fmt::BGR:
        rgbaf = ossia::rgba8{ossia::bgr{v[0], v[1], v[2]}}.dataspace_value;
        break;
      case fmt::HSV:
        rgbaf = ossia::rgba8{ossia::hsv{v[0], v[1], v[2]}}.dataspace_value;
        break;
      case fmt::CMY:
        rgbaf = ossia::rgba8{ossia::cmy8{v[0], v[1], v[2]}}.dataspace_value;
        break;
      default:
        break;
    }

    std::array<uint8_t, 4> rgba;
    rgba[0] = std::clamp(rgbaf[0], 0.f, 255.f);
    rgba[1] = std::clamp(rgbaf[1], 0.f, 255.f);
    rgba[2] = std::clamp(rgbaf[2], 0.f, 255.f);
    rgba[3] = std::clamp(rgbaf[3], 0.f, 255.f);

    send_message(rgba);
  }

  std::function<void(std::array<uint8_t, 4>)> send_message;

  struct ui
  {
    halp_meta(name, "Main")
    halp_meta(layout, halp::layouts::container)
    halp_meta(width, 100)
    halp_meta(height, 100)

    halp::custom_actions_item<color_display> anim{.x = 2, .y = 2};

    struct bus
    {
      void init(ui& ui) { }

      // 4. Receive a message on the UI thread from the processing thread
      static void process_message(ui& self, std::array<uint8_t, 4> msg)
      {
        self.anim.value = msg;
        self.anim.update();
      }
    };
  };
};
}
