#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <examples/Tests/tex_passthrough.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

namespace examples::tests
{
struct TestTexRGBA8Passthrough
{
  halp_meta(name, "Test Texture RGBA8 passthrough")
  halp_meta(c_name, "avnd_test_tex_rgba8")
  halp_meta(category, "Tests/Texture")
  halp_meta(description, "Passthrough of an 8-bit RGBA texture")
  halp_meta(uuid, "a3d37fce-b7c3-44a6-adf1-ab451f6fe5d2")

  struct
  {
    halp::texture_input<"In", halp::rgba_texture> image;
  } inputs;
  struct
  {
    halp::texture_output<"Out", halp::rgba_texture> image;
  } outputs;

  void operator()() { texture_passthrough(inputs.image, outputs.image); }
};
}
