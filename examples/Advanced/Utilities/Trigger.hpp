#pragma once
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>

#include <optional>

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace examples
{
struct Trigger
{
  halp_meta(name, "Trigger")
  halp_meta(c_name, "avnd_trigger")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Send an impulse upon triggering")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#trigger")
  halp_meta(uuid, "37998523-7a8a-4d12-8bd9-37e49adea083")

  struct
  {
    halp::val_port<"Input", float> input;
    halp::range_slider_f32<"ceil"> ceil;
  } inputs;

  struct
  {
    halp::val_port<"Enter", std::optional<halp::impulse>> enter;
    halp::val_port<"Leave", std::optional<halp::impulse>> leave;
  } outputs;

  bool state{};
  int64_t state_enter_frame;
  int64_t state_change_frame;

  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& frames) noexcept
  {
    if(inputs.input.value > inputs.ceil.value.end && !state)
    {
      outputs.enter.value.emplace();
      state = true;
    }
    else if(inputs.input.value < inputs.ceil.value.start && state)
    {
      outputs.leave.value.emplace();
      state = false;
    }
  }
};
}
