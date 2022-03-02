#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/meta.hpp>
#include <avnd/helpers/sample_accurate_controls.hpp>
#include <avnd/helpers/texture.hpp>


namespace examples
{
struct TextureFilterExample
{
  $(name, "My example texture filter");
  $(script_name, "texture_filt");
  $(category, "Demo");
  $(author, "<AUTHOR>");
  $(description, "<DESCRIPTION>");
  $(uuid, "3183d03e-9228-4d50-98e0-e7601dd16a2e");

  struct {
    struct {
      $(name, "In");

      // For inputs we have to request a specific size.
      // The bytes will be provided by an outside mechanism.
      avnd::rgba_texture texture{.width = 500, .height = 500};
    } image;
  } inputs;

  struct {
    struct {
      $(name, "Out");

      avnd::rgba_texture texture;
    } image;
  } outputs;

  // Some place in RAM to store our pixels
  boost::container::vector<unsigned char> bytes;

  // Some state that will stay around, just to make our input texture move a bit
  int k = 0;

  // Some initialization can be done in the constructor.
  TextureFilterExample() noexcept
  {
    // Allocate some initial data
    bytes = avnd::rgba_texture::allocate(500, 500);
    outputs.image.texture.update(bytes.data(), 500, 500);
  }

  void operator()()
  {
    auto& in_tex = inputs.image.texture;

    // Since GPU readbacks are asynchronous: reading textures may take some time and
    // thus the data may not be available from the beginning.
    if(in_tex.bytes == nullptr)
      return;

    // Texture hasn't changed since last time, no need to recompute anything
    if(!in_tex.changed)
      return;
    in_tex.changed = false;

    auto& out_tex = outputs.image.texture;

    const int N = in_tex.width * in_tex.height * 4;
    for(int i = 0; i < N; i++)
      out_tex.bytes[i] = in_tex.bytes[(i * 13 + k * 4) % N];

    // weeeeeee
    k++;

    // Call this when the texture changed
    outputs.image.texture.update(bytes.data(), 500, 500);
  }
};
}
