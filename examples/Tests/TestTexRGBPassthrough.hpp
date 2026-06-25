#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <examples/Tests/tex_passthrough.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

namespace examples::tests
{
struct TestTexRGBPassthrough
{
  halp_meta(name, "Test Texture RGB8 passthrough")
  halp_meta(c_name, "avnd_test_tex_rgb8")
  halp_meta(category, "Tests/Texture")
  halp_meta(description, "Passthrough of a 24-bit RGB texture (no alpha)")
  halp_meta(uuid, "27d27dca-b6c3-47b2-ad1b-724e7376be26")

  struct
  {
    halp::texture_input<"In", halp::rgb_texture> image;
  } inputs;
  struct
  {
    halp::texture_output<"Out", halp::rgb_texture> image;
  } outputs;

  void operator()() { texture_passthrough(inputs.image, outputs.image); }
};
}
