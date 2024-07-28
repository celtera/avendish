#pragma once
#include <cmath>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <functional>
#include <vector>

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace examples
{
struct SumPorts
{
  halp_meta(name, "Sum")
  halp_meta(c_name, "avnd_sumports")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Debug")
  halp_meta(description, "Example of an object with dynamic number of inputs")
  halp_meta(uuid, "48b57b3e-227a-4a55-adce-86c011dcf491")

  struct inputs
  {
    struct : halp::spinbox_i32<"Control", halp::range{0, 10, 0}>
    {
      static std::function<void(SumPorts&, int)> on_controller_interaction()
      {
        return [](SumPorts& object, int value) {
          object.inputs.in.request_port_resize(value);
        };
      }
    } controller;

    struct : halp::knob_f32<"Input {}">
    {
      std::vector<double> ports{};
      std::function<void(int)> request_port_resize;
    } in;
  } inputs;

  struct
  {
    halp::val_port<"Output", float> out;
  } outputs;

  void operator()()
  {
    outputs.out.value = 0;
    int k = 0;

    for(auto val : inputs.in.ports)
      outputs.out.value += std::pow(10, k++) * std::floor(10 * val);
  }
};
}
