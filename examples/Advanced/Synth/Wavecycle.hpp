#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/curve.hpp>
#include <halp/meta.hpp>
#include <halp/smoothers.hpp>

namespace ao
{
struct Wavecycle
{
public:
  halp_meta(name, "Wavecycle")
  halp_meta(c_name, "wavecycle")
  halp_meta(category, "Audio/Generators")
  halp_meta(description, "Generate audio cycles from hand-drawn waveshapes")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/wavecycle.html")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(uuid, "494bd8a3-e973-4fb0-b84b-b4ed3c0068a1")

  struct
  {
    halp::curve_port<"Curve"> curve;
    struct : halp::spinbox_f32<"Frequency", halp::range{1, 20000, 1000}>
    {
      using smooth = halp::milliseconds_smooth<20>;
    } frequency;
  } inputs;

  struct
  {
    halp::audio_channel<"Out", double> audio;
  } outputs;

  int rate{};
  void prepare(halp::setup info) noexcept { this->rate = info.rate; }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical frames) noexcept
  {
    if(inputs.frequency <= 0)
      return;

    const double seconds = 1. / inputs.frequency;
    const double samples = seconds * this->rate;
    const int64_t isamples = std::floor(samples);
    int current_sample = frames.position_in_frames;
    for(int i = 0; i < frames.frames; i++, current_sample++)
    {
      outputs.audio.channel[i]
          = inputs.curve.value.value_at((current_sample % isamples) / double(samples))
            - 0.5;
    }
  }
};
}
