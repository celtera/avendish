#pragma once
#include <cmath>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace examples::helpers
{
struct SumPorts
{
  halp_meta(name, "Sum (helpers)")
  halp_meta(c_name, "avnd_sumports_helpers")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Debug")
  halp_meta(description, "Example of an object with dynamic number of inputs")
  halp_meta(uuid, "191e6e08-a6c0-47d5-bd7b-a0f188cd273c")

  struct inputs
  {
    struct : halp::spinbox_i32<"Control", halp::range{0, 10, 0}>
    {
      static std::function<void(SumPorts&, int)> on_controller_interaction()
      {
        return [](SumPorts& object, int value) {
          object.inputs.in_i.request_port_resize(value);
        };
      }
    } controller;

    halp::dynamic_port<halp::knob_f32<"Input {}">> in_i;
  } inputs;

  struct
  {
    halp::val_port<"Output", float> out;
  } outputs;

  void operator()()
  {
    outputs.out.value = 0;
    int k = 0;

    for(auto val : inputs.in_i.ports)
      outputs.out.value += std::pow(10, k++) * std::floor(10 * val);
  }
};
}
