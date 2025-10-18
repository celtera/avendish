#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples
{
/**
 * This example exhibits a simple multi-channel effect processor.
 */
struct AudioEffectExample
{
  halp_meta(name, "My example effect");
  halp_meta(c_name, "oscr_AudioEffectExample");
  halp_meta(category, "Demo");
  halp_meta(author, "<AUTHOR>");
  halp_meta(description, "<DESCRIPTION>");
  halp_meta(uuid, "c8b57fff-c34c-4772-8f72-fe5267527ece");

  /**
   * Here we have a special case, which happens to be the most common case in audio
   * development. If our inputs start with an audio port of the shape
   *
   *     const double** samples;
   *     std::size_t channels;
   *
   * and our outputs starts with an audio port of shape
   *
   *     double** samples;
   *     std::size_t channels;
   *
   * then it is assumed that we are writing an effect processor, where the outputs
   * should match the inputs. There will be as many output channels as input channels,
   * with enough samples allocated to write from 0 to N.
   *
   * In all the other cases, it is necessary to specify the number of channels for the output
   * as in the Sidechain example.
   */
  struct
  {
    halp::audio_input_bus<"Main Input"> audio;
    halp::knob_f32<"Gain", halp::range{.min = 0.f, .max = 100.f, .init = 10.f}> gain;
  } inputs;

  struct
  {
    halp::audio_output_bus<"Main Output"> audio;
  } outputs;

  /** Most basic effect: multiply N samples of inputs by a gain into equivalent outputs **/
  void operator()(std::size_t N)
  {
    auto& gain = inputs.gain;
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;

    const auto chans = p1.channels;

    // Process the input buffer
    for(std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];

      for(std::size_t j = 0; j < N; j++)
      {
        out[j] = in[j] * gain.value;
      }
    }
  }
};
}
