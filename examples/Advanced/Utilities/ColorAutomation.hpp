#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/gradient_port.hpp>
#include <halp/meta.hpp>
#include <halp/smoothers.hpp>

namespace ao
{
struct ColorAutomation
{
public:
  halp_meta(name, "Color Automation")
  halp_meta(c_name, "color_automation")
  halp_meta(category, "Automations")
  halp_meta(description, "Color automation curve")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(uuid, "80e88817-797c-4ad7-9265-9fefa009a65f")

  struct
  {
    halp::gradient_port<"Curve"> curve;
  } inputs;

  struct
  {
    halp::val_port<"Out", halp::rgba32f_color> value;
  } outputs;

  struct tick { int frames{}; float relative_position{}; };
  void operator()(tick pos) noexcept
  {
    outputs.value = inputs.curve.value.value_at(pos.relative_position);
  }
};

}
