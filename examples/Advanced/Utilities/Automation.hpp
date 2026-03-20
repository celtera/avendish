#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/curve.hpp>
#include <halp/meta.hpp>
#include <halp/smoothers.hpp>

namespace ao
{
struct AutomationTick
{
  int frames{};
  float relative_position{};
};
template <typename CurveType>
struct AutomationBase
{
public:
  halp_meta(name, "Automation")
  halp_meta(c_name, "automation")
  halp_meta(category, "Automations")
  halp_meta(description, "Automation curve")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/automation.html")
  halp_meta(uuid, "77a96254-7348-4cb5-9513-3e16c41e80f0")

  struct
  {
    halp::curve_port<"Curve", CurveType> curve;
  } inputs;

  struct
  {
    halp::val_port<"Out", double> value;
  } outputs;

  using tick = AutomationTick;
  void operator()(tick pos) noexcept
  {
    outputs.value = inputs.curve.value.value_at(pos.relative_position);
  }
};

struct Automation : AutomationBase<halp::custom_curve>
{
};
struct PowerAutomation : AutomationBase<halp::power_curve>
{
};
struct LinearAutomation : AutomationBase<halp::linear_curve>
{
};
}
