#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/curve.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ao
{
/**
 * Plays a hand-drawn cycle; a list or vec sent to the frequency plays one
 * voice per value, mixed. Each voice keeps its own phase, so a frequency
 * change bends the pitch instead of jumping to another point of the cycle.
 */
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

  static constexpr int max_voices = 64;

  struct
  {
    halp::curve_port<"Curve"> curve;
    struct : halp::spinbox_f32<"Frequency", halp::range{1, 20000, 220}>
    {
      // One frequency per voice when a list or a vec is sent.
      std::vector<float> list;
    } frequency;
  } inputs;

  struct
  {
    halp::audio_channel<"Out", double> audio;
  } outputs;

  struct voice
  {
    double phase{};
    double frequency{};
    double target_frequency{};
    double gain{};
    double target_gain{};
  };
  std::array<voice, max_voices> voices{};

  double rate{48000.};
  double frequency_smooth{};
  double gain_smooth{};
  double norm{1.};
  int64_t expected_position{-1};

  void prepare(halp::setup info) noexcept
  {
    this->rate = info.rate > 0 ? info.rate : 48000.;
    // One-pole smoothing: 20 ms for the pitch, 5 ms for voice gains.
    constexpr double two_pi = 6.283185307179586;
    frequency_smooth = std::exp(-two_pi / (20e-3 * rate));
    gain_smooth = std::exp(-two_pi / (5e-3 * rate));
    inputs.frequency.list.reserve(max_voices);
  }

  using tick = halp::tick_musical;
  void operator()(halp::tick_musical frames) noexcept
  {
    auto& list = inputs.frequency.list;
    const int count
        = list.empty() ? 1 : std::min<int>(int(list.size()), max_voices);
    auto frequency_of = [&](int i) -> double {
      return list.empty() ? inputs.frequency.value : list[i];
    };

    // On a transport jump, each phase is set as if the voice had played at its
    // frequency since position 0.
    const int64_t position = frames.position_in_frames;
    const bool jumped = position != expected_position;
    expected_position = position + frames.frames;

    int playing = 0;
    for(int i = 0; i < max_voices; i++)
    {
      auto& v = voices[i];
      const double f = i < count ? frequency_of(i) : 0.;
      if(i < count && f > 0. && f < rate / 2.)
      {
        // A silent voice starts at its frequency instead of gliding to it.
        if(v.gain <= 0. && v.target_gain <= 0.)
          v.frequency = f;
        v.target_frequency = f;
        v.target_gain = 1.;
        playing++;
      }
      else
      {
        v.target_gain = 0.;
      }
      if(jumped && (v.target_gain > 0. || v.gain > 0.))
      {
        const double cycles = double(position) * v.frequency / rate;
        v.phase = cycles - std::floor(cycles);
      }
    }
    const double target_norm = 1. / std::max(1, playing);

    auto& curve = inputs.curve.value;
    double* out = outputs.audio.channel;
    for(int s = 0; s < frames.frames; s++)
    {
      norm = target_norm - gain_smooth * (target_norm - norm);
      double sample = 0.;
      for(auto& v : voices)
      {
        if(v.gain <= 0. && v.target_gain <= 0.)
          continue;
        v.gain = v.target_gain - gain_smooth * (v.target_gain - v.gain);
        if(v.target_gain <= 0. && v.gain < 1e-5)
          v.gain = 0.;
        v.frequency
            = v.target_frequency - frequency_smooth * (v.target_frequency - v.frequency);

        sample += v.gain * (curve.value_at(v.phase) - 0.5);

        v.phase += v.frequency / rate;
        v.phase -= std::floor(v.phase);
      }
      out[s] = sample * norm;
    }
  }
};
}
