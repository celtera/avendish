#pragma once
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>
#include <halp/meta.hpp>

namespace ao
{
/**
 * @brief Basic 4-channel mixer. Not using any intelligent pan law
 */
struct StereoMixer
{
public:
  halp_meta(name, "Stereo Mixer")
  halp_meta(c_name, "ao_stereomixer")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Audio/Utilities")
  halp_meta(description, "Mix and pan multiple stereo audio inputs")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/audio-utilities.html#stereo-mixer")
  halp_meta(uuid, "93910fcf-e45a-4eac-ad55-a14b221a12aa")

  struct
  {
    halp::dynamic_audio_bus<"In 1", double> a0;
    halp::dynamic_audio_bus<"In 2", double> a1;
    halp::dynamic_audio_bus<"In 3", double> a2;
    halp::dynamic_audio_bus<"In 4", double> a3;

    halp::knob_f32<"Gain 1"> c0g;
    halp::knob_f32<"Pan 1"> c0p;
    halp::knob_f32<"Gain 2"> c1g;
    halp::knob_f32<"Pan 2"> c1p;
    halp::knob_f32<"Gain 3"> c2g;
    halp::knob_f32<"Pan 3"> c2p;
    halp::knob_f32<"Gain 4"> c3g;
    halp::knob_f32<"Pan 4"> c3p;
  } inputs;

  struct
  {
    halp::fixed_audio_bus<"Out", double, 2> audio;
  } outputs;

  void mix(double* HALP_RESTRICT * HALP_RESTRICT samples, int channels, double gain, double pan, int i, double* HALP_RESTRICT l, double* HALP_RESTRICT r)
  {
    const auto lp = gain * (1. - pan);
    const auto rp = gain * pan;
    switch(channels)
    {
      case 0:
        break;
      case 1:
        l[i] += samples[0][i] * lp;
        r[i] += samples[0][i] * rp;
        break;
      case 2:
        l[i] += samples[0][i] * lp;
        r[i] += samples[1][i] * rp;
        break;
      default:
      {
        double res = 0.;
        for(int c = 0; c < channels; c++)
          res += samples[c][i];
        l[i] += res * lp;
        r[i] += res * rp;
        break;
      }
    }
  }

  void operator()(int frames) noexcept
  {
    auto* l = outputs.audio[0];
    auto* r = outputs.audio[1];

#pragma omp simd
    for(int i = 0; i < frames; i++)
    {
      l[i] = 0.;
      r[i] = 0.;
      mix(inputs.a0.samples, inputs.a0.channels, inputs.c0g, inputs.c0p, i, l, r);
      mix(inputs.a1.samples, inputs.a1.channels, inputs.c1g, inputs.c1p, i, l, r);
      mix(inputs.a2.samples, inputs.a2.channels, inputs.c2g, inputs.c2p, i, l, r);
      mix(inputs.a3.samples, inputs.a3.channels, inputs.c3g, inputs.c3p, i, l, r);
    }
  }
};

}
