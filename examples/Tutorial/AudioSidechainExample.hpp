#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples
{
/**
 * This example exhibits a more advanced multi-channel processor, with side-chain inputs and outputs.
 */
struct AudioSidechainExample
{
  halp_meta(name, "Sidechain example");
  halp_meta(c_name, "oscr_AudioSidechainExample");
  halp_meta(category, "Demo");
  halp_meta(author, "<AUTHOR>");
  halp_meta(description, "<DESCRIPTION>");
  halp_meta(uuid, "6fcfa5ad-ac5e-4851-a7bc-72f6fbf57dcd");

  struct
  {
    halp::audio_input_bus<"Main Input"> audio;
    halp::audio_input_bus<"Sidechain"> sidechain;

    halp::knob_f32<"Gain", halp::range{.min = 0.f, .max = 100.f, .init = 10.f}> gain;
  } inputs;

  struct
  {
    // Here we say: use the same number of channels than the input "Main Input" defined above.
    halp::mimic_audio_bus<"Output", &decltype(inputs)::audio> audio;

    // Add a side-chain which will always be a mono channel
    halp::fixed_audio_bus<"Mono downmix", double, 1> side_out;
  } outputs;

  void operator()(std::size_t N)
  {
    auto& gain = inputs.gain;
    auto& p1 = inputs.audio;
    auto& sc = inputs.sidechain;
    auto& p2 = outputs.audio;
    auto& mono = outputs.side_out.samples[0];

    const std::size_t chans = p1.channels;

    // Process the input buffer
    for(std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];

      // If there's enough channels in the sidechain, use it
      if(sc.channels > i)
      {
        auto& sidechain = sc.samples[i];
        for(std::size_t j = 0; j < N; j++)
        {
          out[j] = std::abs(sidechain[j] * gain.value) > 0.5 ? in[j] * gain.value : 0.0;
          mono[j] += out[j];
        }
      }
      else
      {
        // Don't use the sidechain
        for(std::size_t j = 0; j < N; j++)
        {
          out[j] = in[j] * gain.value;
          mono[j] += out[j];
        }
      }
    }
  }
};
}
