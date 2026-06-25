#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <examples/Tests/tex_passthrough.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

namespace examples::tests
{
struct TestTexR32FPassthrough
{
  halp_meta(name, "Test Texture R32F passthrough")
  halp_meta(c_name, "avnd_test_tex_r32f")
  halp_meta(category, "Tests/Texture")
  halp_meta(description, "Passthrough of a single-channel 32-bit float texture")
  halp_meta(uuid, "714f4380-e01a-4bd0-9a2f-5a4655e12c63")

  struct
  {
    halp::texture_input<"In", halp::r32f_texture> image;
  } inputs;
  struct
  {
    halp::texture_output<"Out", halp::r32f_texture> image;
  } outputs;

  void operator()() { texture_passthrough(inputs.image, outputs.image); }
};
}
