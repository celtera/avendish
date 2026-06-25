#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

namespace examples::tests
{
struct TestTexVariableInput
{
  halp_meta(name, "Test Texture variable-format input")
  halp_meta(c_name, "avnd_test_tex_variable")
  halp_meta(category, "Tests/Texture")
  halp_meta(description, "Reports the size of a host-format-chosen texture")
  halp_meta(uuid, "af4ded9e-c09b-488f-be0c-5f336a07b700")

  struct
  {
    halp::texture_input<"In", halp::custom_variable_texture> image;
  } inputs;
  struct
  {
    halp::hbargraph_f32<"Width", halp::range{0., 8192., 0.}> width;
    halp::hbargraph_f32<"Height", halp::range{0., 8192., 0.}> height;
  } outputs;

  void operator()()
  {
    outputs.width.value = inputs.image.texture.width;
    outputs.height.value = inputs.image.texture.height;
  }
};
}
