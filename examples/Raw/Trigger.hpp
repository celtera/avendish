#pragma once
#include <optional>

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace examples
{
struct Trigger
{
  static consteval auto name() { return "Trigger"; }
  static consteval auto c_name() { return "avnd_trigger"; }
  static constexpr auto author() { return "Jean-Michaël Celerier"; }
  static constexpr auto category() { return "Control/Mappings"; }
  static constexpr auto description() { return "Send an impulse upon triggering"; }
  static consteval auto uuid() { return "37998523-7a8a-4d12-8bd9-37e49adea083"; }

  struct
  {
    struct
    {
      static constexpr auto name() { return "Input"; }
      float value;
    } input;
    struct
    {
      static constexpr auto name() { return "Ceil"; }
      float value;
      enum widget
      {
        hslider
      };
      struct range
      {
        float min = 0.f, max = 1.f, init = 0.5f;
      };
    } ceil;
  } inputs;

  struct
  {
    struct
    {
      std::optional<bool> value;
    } enter;
    struct
    {
      std::optional<bool> value;
    } leave;
  } outputs;

  bool state{};
  void operator()()
  {
    if(inputs.input.value > inputs.ceil.value && !state)
    {
      outputs.enter.value = true;
      state = true;
    }
    else if(inputs.input.value < inputs.ceil.value && state)
    {
      outputs.leave.value = true;
      state = false;
    }
  }
};
}
