#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/curve.hpp>
#include <halp/meta.hpp>
#include <smallfun_trivial.hpp>

namespace ao
{
struct Automation
{
public:
  halp_meta(name, "Automation")
  halp_meta(c_name, "automation")
  halp_meta(category, "Automation/Float")
  halp_meta(description, "An automation curve")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(uuid, "494bd8a3-e973-4fb0-b84b-b4ed3c0068a1")

  struct
  {
    struct
    {
      halp::custom_curve curve;
    } automation;
  } inputs;

  struct
  {
    struct
    {
      float value{};
    } out;
  } outputs;

  void prepare(halp::setup info) noexcept { }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical frames) noexcept
  {
    outputs.out.value
        = inputs.automation.curve.value_at(frames.position_in_frames / (48000 * 10.));
  }

private:
};
}
