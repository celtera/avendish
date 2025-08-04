#pragma once
#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>
#include <halp/texture.hpp>

namespace examples
{
struct TextureFilterExample
{
  halp_meta(name, "My example texture filter");
  halp_meta(c_name, "texture_filt");
  halp_meta(category, "Demo");
  halp_meta(author, "Jean-Michaël Celerier");
  halp_meta(description, "Example texture filter");
  halp_meta(uuid, "3183d03e-9228-4d50-98e0-e7601dd16a2e");

  struct
  {
    halp::texture_input<"In"> image;
    halp::knob_f32<"Gain", halp::range{0., 255., 0.}> gain;
    halp::knob_i32<"Downscale", halp::range{1, 32, 8}> downscale;
  } inputs;

  struct
  {
    halp::texture_output<"Out"> image;
  } outputs;

  // Some initialization can be done in the constructor.
  TextureFilterExample() noexcept
  {
    // Allocate some initial data
    outputs.image.create(1, 1);
  }

  void operator()()
  {
    auto& in_tex = inputs.image.texture;
    auto& out_tex = outputs.image.texture;

    // Since GPU readbacks are asynchronous: reading textures may take some time and
    // thus the data may not be available from the beginning.
    if(in_tex.bytes == nullptr)
      return;

    // Texture hasn't changed since last time, no need to recompute anything
    if(!in_tex.changed)
      return;
    in_tex.changed = false;

    const double downscale_factor = inputs.downscale;

    const int small_w = in_tex.width / downscale_factor;
    const int small_h = in_tex.height / downscale_factor;
    // We (dirtily) downscale by a factor of downscale_factor
    if(out_tex.width != small_w || out_tex.height != small_h)
      outputs.image.create(small_w, small_h);

    for(int y = 0; y < small_h; y++)
    {
      for(int x = 0; x < small_w; x++)
      {
        // Get a pixel
        auto [r, g, b, a] = inputs.image.get(
            std::floor(x * downscale_factor), std::floor(y * downscale_factor));

        // (Dirtily) Take the luminance and compute its contrast
        double contrasted = std::pow((r + g + b) / (3. * 255.), 4.);

        // (Dirtily) Posterize
        uint8_t col
            = std::clamp(uint8_t(contrasted * 8) * (255 / 8.) * inputs.gain, 0., 255.);

        // Update the output texture
        outputs.image.set(x, y, col, col, col, 255);
      }
    }

    // Call this when the texture changed
    outputs.image.upload();
  }
};
}
