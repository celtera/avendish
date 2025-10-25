#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/meta.hpp>
#include <halp/smooth_controls.hpp>
namespace ao
{
class MonoMix
{
public:
  halp_meta(name, "Mono mix 8")
  halp_meta(c_name, "mono_mix")
  halp_meta(category, "Audio/Utilities")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Sum all the input channels")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/audio-utilities.html#mono-mix")
  halp_meta(uuid, "a3b16235-5855-43a6-8a2f-fc7d6f3a8038")

  struct
  {
    halp::dynamic_audio_bus<"1", double> c0;
    halp::dynamic_audio_bus<"2", double> c1;
    halp::dynamic_audio_bus<"3", double> c2;
    halp::dynamic_audio_bus<"4", double> c3;
    halp::dynamic_audio_bus<"5", double> c4;
    halp::dynamic_audio_bus<"6", double> c5;
    halp::dynamic_audio_bus<"7", double> c6;
    halp::dynamic_audio_bus<"8", double> c7;

    halp::smooth_knob<"Gain 1", halp::range{0., 1., 0.}> g0;
    halp::smooth_knob<"Gain 2", halp::range{0., 1., 0.}> g1;
    halp::smooth_knob<"Gain 3", halp::range{0., 1., 0.}> g2;
    halp::smooth_knob<"Gain 4", halp::range{0., 1., 0.}> g3;
    halp::smooth_knob<"Gain 5", halp::range{0., 1., 0.}> g4;
    halp::smooth_knob<"Gain 6", halp::range{0., 1., 0.}> g5;
    halp::smooth_knob<"Gain 7", halp::range{0., 1., 0.}> g6;
    halp::smooth_knob<"Gain 8", halp::range{0., 1., 0.}> g7;
  } inputs;

  struct
  {
    halp::fixed_audio_bus<"Output", double, 1> audio;
  } outputs;

  void operator()(const int frames)
  {
    auto out = outputs.audio.channel(0, frames);

#pragma omp simd
    for(int j = 0; j < frames; j++)
      out[j] = 0.;

    all_input_channels.clear();
    all_input_gains.clear();

    const halp::dynamic_audio_bus_base<double> ports[8] = {
        inputs.c0, inputs.c1, inputs.c2, inputs.c3,
        inputs.c4, inputs.c5, inputs.c6, inputs.c7,
    };
    const double gains[8] = {
        inputs.g0.value, inputs.g1.value, inputs.g2.value, inputs.g3.value,
        inputs.g4.value, inputs.g5.value, inputs.g6.value, inputs.g7.value,
    };

    for(int p = 0; p < 8; p++)
    {
      auto& port = ports[p];
      auto gain = gains[p];
      for(int i = 0; i < port.channels; i++)
      {
        if(auto chan = port.channel(i, frames); !chan.empty())
        {
          all_input_channels.push_back(chan.data());
          all_input_gains.push_back(gain);
        }
      }
    }

    {
      const auto N = all_input_channels.size();
#pragma omp simd
      for(std::size_t c = 0; c < N; c++)
      {
        const double* const in = all_input_channels[c];
        const double g = all_input_gains[c];
        for(int j = 0; j < frames; j++)
          out[j] += in[j] * g;
      }
    }
  }

  std::vector<double*> all_input_channels;
  std::vector<double> all_input_gains;

  MonoMix()
  {
    all_input_channels.reserve(64);
    all_input_gains.reserve(64);
  }
};
}
