#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/concepts/parameter.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>
#include <halp/texture.hpp>

namespace examples
{
/**
 * The next part is about working with the video system.
 *
 * Note that this is a very inefficient way of doing generation / processing
 * of video data, as this is done on the CPU ; this should
 * only ever be used if the algorithm is not implementable as a GPU shader,
 * with the ISF format that score supports.
 * For instance, if you want to use some OpenCV or Dlib goodness.
 *
 * Note that the API is a bit more limited, as in this case the nodes
 * run in the GPU processing thread, at the screen / output's frame rate, instead
 * of running in the audio thread, at audio buffer rate.
 *
 * In particular, currently, timing information is not accessible, and it is only
 * possible to have an output texture.
 */
struct TextureGeneratorExample
{
  halp_meta(name, "My example texture generator");
  halp_meta(c_name, "texture_gen");
  halp_meta(category, "Demo");
  halp_meta(author, "<AUTHOR>");
  halp_meta(description, "<DESCRIPTION>");
  halp_meta(uuid, "01247f4f-6b19-458d-845d-9f7cc2d9d663");

  // By know you know the drill: define inputs, outputs...
  struct
  {
    halp::hslider_f32<"Bamboozling", halp::range{0.0001, 0.1, 0.01}> bamboozle;
  } inputs;

  struct
  {
    // See TextureFilterExample for helper types which abstract the allocation side of things
    struct
    {
      halp_meta(name, "Out");

      // This type is a view on a texture
      halp::rgba_texture texture;
    } image;
  } outputs;

  // Some place in RAM to store our pixels
  halp::rgba_texture::uninitialized_bytes bytes;

  TextureGeneratorExample()
  {
    // Allocate some initial data
    bytes = halp::rgba_texture::allocate(480, 270);
    for (unsigned char& c : bytes)
    {
      c = std::rand() % 10;
    }
  }

  // Note that as soon as we use textures,
  // we run at frame rate (e.g. 60hz) instead of audio buffer rate
  // (e.g. maybe 1000hz if you have a decent enough soundcard).
  void operator()()
  {
    // Do some magic
    int k = 0;
    for (unsigned char& c : bytes)
    {
      c += k++ * inputs.bamboozle.value;
    }

    // Call this when the texture changed
    outputs.image.texture.update(bytes.data(), 480, 270);
  }
};
}
