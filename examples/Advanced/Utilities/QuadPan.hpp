#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/soundfile_port.hpp>

#include <numbers>

namespace ao
{
class QuadPan
{
public:
  halp_meta(name, "Quad Pan")
  halp_meta(c_name, "quad_pan")
  halp_meta(category, "Audio/Spatialization")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Mix between front / back and left / right")
  halp_meta(uuid, "8761cb8b-f404-4161-813f-f7b769e8e611")

  struct
  {
    halp::dynamic_audio_bus<"1", double> audio;

    halp::knob_f32<"Gain"> gain;
    halp::knob_f32<"L/R"> lr;
    halp::knob_f32<"F/B"> fb;
  } inputs;

  struct
  {
    halp::fixed_audio_bus<"FL", double, 1> fl;
    halp::fixed_audio_bus<"FR", double, 1> fr;
    halp::fixed_audio_bus<"BL", double, 1> bl;
    halp::fixed_audio_bus<"BR", double, 1> br;
  } outputs;

  void operator()(const int frames)
  {
    const auto in = inputs.audio.channel(0, frames);
    const auto g = inputs.gain.value;

    auto fl = outputs.fl.channel(0, frames);
    auto fr = outputs.fr.channel(0, frames);
    auto bl = outputs.bl.channel(0, frames);
    auto br = outputs.br.channel(0, frames);
    const avnd::span<double> channels[4] = {fl, fr, bl, br};

#pragma omp simd
    for(auto chan : channels)
      for(int j = 0; j < frames; j++)
        chan[j] = 0.;

    const auto lr_c = std::cos(inputs.lr + 1.) * 0.25 * std::numbers::pi;
    const auto lr_s = std::sin(inputs.lr + 1.) * 0.25 * std::numbers::pi;
    const auto fb_c = std::cos(inputs.fb + 1.) * 0.25 * std::numbers::pi;
    const auto fb_s = std::sin(inputs.fb + 1.) * 0.25 * std::numbers::pi;

#pragma omp simd
    for(int i = 0; i < frames; i++)
    {
      const auto in_sample = in[i] * g;
      channels[0][i] = in_sample * lr_c * fb_c;
      channels[1][i] = in_sample * lr_s * fb_c;
      channels[2][i] = in_sample * lr_c * fb_s;
      channels[3][i] = in_sample * lr_s * fb_s;
    }
  }
};
}
