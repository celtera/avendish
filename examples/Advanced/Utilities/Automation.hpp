#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/curve.hpp>
#include <halp/meta.hpp>
#include <halp/smoothers.hpp>

namespace ao
{
struct Automation
{
public:
  halp_meta(name, "Automation")
  halp_meta(c_name, "automation")
  halp_meta(category, "Automations")
  halp_meta(description, "Automation curve")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(uuid, "77a96254-7348-4cb5-9513-3e16c41e80f0")

  struct
  {
    halp::curve_port<"Curve"> curve;
  } inputs;

  struct
  {
    halp::val_port<"Out", double> value;
  } outputs;

  struct tick
  {
    int frames{};
    float relative_position{};
  };
  void operator()(tick pos) noexcept
  {
    outputs.value = inputs.curve.value.value_at(pos.relative_position);
  }
};
}
