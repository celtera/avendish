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

  enum Mode
  {
    Outside,
    Inside
  };
  struct
  {
    halp::val_port<"Input", float> input;
    halp::range_slider_f32<"Ceil"> ceil;
    struct : halp::enum_t<Mode, "Mode">
    {
      enum widget
      {
        combobox
      };
      struct range
      {
        std::string_view values[2] = {"Above/Below", "Outside/Inside"};
        Mode init{};
      };
    } mode;
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
  void prepare()
  {
    inputs.input.value = inputs.ceil.value.start;
    outputs.enter.value.reset();
    outputs.leave.value.reset();
  }

  void operator()(const halp::tick_musical& frames) noexcept
  {
    if(inputs.mode == Mode::Outside)
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
    else if(inputs.mode == Mode::Inside)
    {
      if(inputs.input.value >= inputs.ceil.value.start
         && inputs.input.value <= inputs.ceil.value.end && !state)
      {
        outputs.enter.value.emplace();
        state = true;
      }
      else if(
          (inputs.input.value < inputs.ceil.value.start
           || inputs.input.value > inputs.ceil.value.end)
          && state)
      {
        outputs.leave.value.emplace();
        state = false;
      }
    }
  }
};
}
