#pragma oce
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
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
  halp_meta(author, "<AUTHOR>");
  halp_meta(description, "<DESCRIPTION>");
  halp_meta(uuid, "3183d03e-9228-4d50-98e0-e7601dd16a2e");

  struct
  {
    halp::texture_input<"In"> image;
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
    if (in_tex.bytes == nullptr)
      return;

    // Texture hasn't changed since last time, no need to recompute anything
    if (!in_tex.changed)
      return;
    in_tex.changed = false;

    // We (dirtily) downscale by a factor of 16
    if (out_tex.width != in_tex.width || out_tex.height != in_tex.height)
      outputs.image.create(in_tex.width / 16, in_tex.height / 16);

    for (int y = 0; y < in_tex.height / 16; y++)
    {
      for (int x = 0; x < in_tex.width / 16; x++)
      {
        // Get a pixel
        auto [r, g, b, a] = inputs.image.get(x * 16, y * 16);

        // (Dirtily) Take the luminance and compute its contrast
        double contrasted = std::pow((r + g + b) / (3. * 255.), 4.);

        // (Dirtily) Posterize
        uint8_t col = uint8_t(contrasted * 8) * (255 / 8.);

        // Update the output texture
        outputs.image.set(x, y, col, col, col, 255);
      }
    }

    // Call this when the texture changed
    outputs.image.upload();
  }
};
}
