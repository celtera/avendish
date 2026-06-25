#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <cstdint>

namespace examples::tests
{
struct TestTexGenerator
{
  halp_meta(name, "Test Texture generator")
  halp_meta(c_name, "avnd_test_tex_generator")
  halp_meta(category, "Tests/Texture")
  halp_meta(description, "Animated RGBA gradient generator (output only)")
  halp_meta(uuid, "2764cc03-939e-4a71-822d-f0536118e905")

  struct
  {
    halp::knob_i32<"Width", halp::range{1, 1024, 64}> width;
    halp::knob_i32<"Height", halp::range{1, 1024, 64}> height;
  } inputs;
  struct
  {
    halp::texture_output<"Out", halp::rgba_texture> image;
  } outputs;

  int frame = 0;

  void operator()()
  {
    const int w = inputs.width;
    const int h = inputs.height;
    auto& out = outputs.image.texture;
    if(out.width != w || out.height != h)
      outputs.image.create(w, h);
    if(out.bytes == nullptr)
      return;

    const uint8_t phase = static_cast<uint8_t>(frame++);
    for(int y = 0; y < h; y++)
      for(int x = 0; x < w; x++)
        outputs.image.set(
            x, y, static_cast<uint8_t>(x * 255 / w),
            static_cast<uint8_t>(y * 255 / h), phase, 255);
    outputs.image.upload();
  }
};
}
