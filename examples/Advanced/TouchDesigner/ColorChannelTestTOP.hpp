#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Color-channel diagnostic TOP.
 *
 * Purpose: figure out whether the red/blue channel swap observed with the
 * TouchDesigner TOP binding happens on the INPUT (TD texture -> avnd) or on the
 * OUTPUT (avnd -> TD texture) side.
 *
 * It has two modes, selected by the "Mode" parameter:
 *
 *  - Generate (0): ignores any input and writes a fixed, known pattern. This
 *    exercises ONLY the output path, so it tells us whether OUTPUT is correct.
 *    The pattern is four quadrants of known, R/B-asymmetric colors:
 *
 *        +-----------------+-----------------+
 *        |  RED            |  GREEN          |
 *        |  (255,  0,  0)  |  (  0,255,  0)  |
 *        +-----------------+-----------------+
 *        |  BLUE           |  ORANGE         |
 *        |  (  0,  0,255)  |  (255,128,  0)  |
 *        +-----------------+-----------------+
 *
 *    Read the result in TD:
 *      * Top-left quadrant shows RED, bottom-left shows BLUE, bottom-right
 *        shows ORANGE  -> OUTPUT path is correct.
 *      * Top-left shows BLUE, bottom-left shows RED, bottom-right shows AZURE
 *        (0,128,255)   -> OUTPUT path swaps R and B.
 *      (Green/top-right is symmetric under an R/B swap, so judge by the red,
 *       blue and orange quadrants.)
 *
 *  - Passthrough (1): copies the input texture to the output unchanged, reading
 *    each pixel with the avnd `get()` accessor and writing it back with `set()`.
 *    This exercises INPUT then OUTPUT (the full round trip).
 *
 * Diagnosis procedure:
 *   1. Drop this TOP with no input, Mode = Generate. Confirm whether OUTPUT is
 *      correct using the quadrant key above.
 *   2. Feed a known solid color into the TOP (e.g. a Constant TOP set to pure
 *      red 1,0,0) and set Mode = Passthrough.
 *        - If Generate was correct (output OK) but Passthrough turns the red
 *          input blue  -> the swap is on INPUT.
 *        - If Generate already showed the swap                 -> the swap is on
 *          OUTPUT (and Passthrough double-swaps back to correct).
 *        - If both are correct                                  -> no swap.
 */

#include <halp/controls.enums.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <cstdint>

namespace examples::touchdesigner
{

struct ColorChannelTestTOP
{
  halp_meta(name, "Color Channel Test");
  halp_meta(c_name, "avnd_color_channel_test");
  halp_meta(category, "Debug");
  halp_meta(author, "Avendish");
  halp_meta(description, "Diagnoses R/B channel swap on TD TOP input vs output");
  halp_meta(uuid, "f1c2e3a4-5b6d-4e7f-8a9b-0c1d2e3f4a5b");

  enum class Mode
  {
    Generate,
    Passthrough
  };

  struct
  {
    halp::texture_input<"Input"> image;
    halp::enum_t<Mode, "Mode"> mode;
    // Output size used in Generate mode (Passthrough follows the input size).
    halp::knob_i32<"Gen Width", halp::range{16, 4096, 512}> gen_width;
    halp::knob_i32<"Gen Height", halp::range{16, 4096, 512}> gen_height;
  } inputs;

  struct
  {
    halp::texture_output<"Output"> image;
  } outputs;

  int last_width{0};
  int last_height{0};

  ColorChannelTestTOP() { outputs.image.create(512, 512); }

  void generate()
  {
    const int w = inputs.gen_width.value;
    const int h = inputs.gen_height.value;
    if(w != last_width || h != last_height)
    {
      outputs.image.create(w, h);
      last_width = w;
      last_height = h;
    }

    const int hw = w / 2;
    const int hh = h / 2;
    for(int y = 0; y < h; ++y)
    {
      for(int x = 0; x < w; ++x)
      {
        const bool left = x < hw;
        const bool top = y < hh;

        std::uint8_t r = 0, g = 0, b = 0;
        if(top && left)        { r = 255; g = 0;   b = 0;   } // RED
        else if(top && !left)  { r = 0;   g = 255; b = 0;   } // GREEN
        else if(!top && left)  { r = 0;   g = 0;   b = 255; } // BLUE
        else                   { r = 255; g = 128; b = 0;   } // ORANGE

        outputs.image.set(x, y, r, g, b, 255);
      }
    }
    outputs.image.upload();
  }

  void passthrough()
  {
    auto& in_tex = inputs.image.texture;
    if(in_tex.bytes == nullptr || in_tex.width <= 0 || in_tex.height <= 0)
      return;

    if(outputs.image.texture.width != in_tex.width
       || outputs.image.texture.height != in_tex.height)
    {
      outputs.image.create(in_tex.width, in_tex.height);
    }

    for(int y = 0; y < in_tex.height; ++y)
    {
      for(int x = 0; x < in_tex.width; ++x)
      {
        auto [r, g, b, a] = inputs.image.get(x, y);
        outputs.image.set(x, y, r, g, b, a);
      }
    }
    outputs.image.upload();
  }

  void operator()()
  {
    switch(inputs.mode.value)
    {
      case Mode::Generate:
        generate();
        break;
      case Mode::Passthrough:
        passthrough();
        break;
    }
  }
};

} // namespace examples::touchdesigner
