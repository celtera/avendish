#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>
#include <halp/tensor_port.hpp>

namespace examples::tests
{
struct TestTensorInput
{
  halp_meta(name, "Test tensor input")
  halp_meta(c_name, "avnd_test_tensor")
  halp_meta(category, "Tests/Tensor")
  halp_meta(uuid, "ee113315-8e11-4a09-937c-ea2e03175b46")
  struct { halp::tensor_port<"In", halp::tensor_view<float>> tensor; } inputs;
  struct { halp::val_port<"Rank", int> rank; } outputs;
  void operator()()
  {
    outputs.rank.value = (int)std::ssize(inputs.tensor.shape());
  }
};
}
