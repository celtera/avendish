#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

#include <vector>

namespace examples::tests
{
struct TestMultiSlider
{
  halp_meta(name, "Test multi slider")
  halp_meta(c_name, "avnd_test_multislider")
  halp_meta(category, "Tests/Controls")
  halp_meta(uuid, "ff3d57de-7fff-46c9-91bc-28e8df96fe8b")
  struct
  {
    struct
    {
      halp_meta(name, "In")
      enum widget { multi_slider };
      std::vector<float> value;
    } in;
  } inputs;
  struct { halp::val_port<"Count", int> count; } outputs;
  void operator()() { outputs.count.value = (int)inputs.in.value.size(); }
};
}
