/* SPDX-License-Identifier: GPL-3.0-or-later */

// Polyphony / channel-count contract for mono and per-sample processors.
//
// A mono processor is replicated once per channel: the container holds one
// instance per channel and each instance sees only its own channel's samples.
// That replication is set up by effect_container::init_channels(), and a
// binding that calls it with the object's *compile-time* channel count instead
// of the count it actually negotiated ends up with a single instance -- every
// channel then runs through instance 0, so channel 1 comes out a copy of
// channel 0 and per-channel state is shared. That is exactly what the Max
// binding did with a multichannel inlet.
//
// These tests pin the contract at the level where it is observable, without
// needing any host.

#include <catch2/catch_all.hpp>

#include <avnd/concepts/processor.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/wrappers/prepare.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <vector>

namespace
{
// Stateless: proves each channel is processed from its OWN input samples.
struct MonoGain
{
  halp_meta(name, "Mono gain")
  halp_meta(c_name, "test_mono_gain")
  halp_meta(uuid, "6f0a7a0e-2b1d-4e0a-9a3e-1a4a4f9d2f01")

  struct
  {
    halp::hslider_f32<"Gain", halp::range{.min = 0., .max = 2., .init = 1.}> gain;
  } inputs;

  float operator()(float in) { return in * inputs.gain; }
};

// Stateful: proves the per-channel instances are INDEPENDENT. A running sum
// shared between channels would leak channel 0's history into channel 1.
struct MonoAccumulator
{
  halp_meta(name, "Mono accumulator")
  halp_meta(c_name, "test_mono_accum")
  halp_meta(uuid, "6f0a7a0e-2b1d-4e0a-9a3e-1a4a4f9d2f02")

  struct
  {
  } inputs;

  float running_sum{};
  float operator()(float in)
  {
    running_sum += in;
    return running_sum;
  }
};

// Drive `T` through the same adapter every audio binding uses, with `channels`
// distinct input channels, and return the produced output channels.
template <typename T>
std::vector<std::vector<float>>
run(int channels, int frames, const std::vector<std::vector<float>>& input)
{
  avnd::effect_container<T> effect;
  avnd::process_adapter<T> processor;

  avnd::process_setup setup{
      .input_channels = channels,
      .output_channels = channels,
      .frames_per_buffer = frames,
      .rate = 44100.};

  processor.allocate_buffers(setup, float{});
  effect.init_channels(channels, channels);
  avnd::prepare(effect, setup);

  std::vector<std::vector<float>> ins = input;
  std::vector<std::vector<float>> outs(channels, std::vector<float>(frames, 0.f));
  std::vector<float*> in_ptrs, out_ptrs;
  for(auto& c : ins)
    in_ptrs.push_back(c.data());
  for(auto& c : outs)
    out_ptrs.push_back(c.data());

  processor.process(
      effect, avnd::span<float*>{in_ptrs.data(), std::size_t(channels)},
      avnd::span<float*>{out_ptrs.data(), std::size_t(channels)}, frames);
  return outs;
}

// Two channels carrying clearly different signals, so a channel confusion
// cannot pass by coincidence.
std::vector<std::vector<float>> distinct_inputs(int channels, int frames)
{
  std::vector<std::vector<float>> in(channels, std::vector<float>(frames));
  for(int c = 0; c < channels; c++)
    for(int i = 0; i < frames; i++)
      in[c][i] = float(c + 1) * (1.f + float(i));
  return in;
}
}

TEST_CASE("every channel is processed, whatever the channel count", "[polyphony]")
{
  // The defect this pins: initialising the polyphony with the compile-time
  // channel count (1, for a mono object) leaves a single instance, and the
  // channels above the first are never processed from their own data.
  constexpr int frames = 8;
  for(int channels : {1, 2, 4})
  {
    const auto in = distinct_inputs(channels, frames);
    const auto out = run<MonoGain>(channels, frames, in);

    REQUIRE(out.size() == std::size_t(channels));
    for(int c = 0; c < channels; c++)
      REQUIRE(out[c][frames - 1] == Catch::Approx(in[c][frames - 1]));
  }
}

TEST_CASE("a mono processor reads each channel's own samples", "[polyphony]")
{
  constexpr int channels = 2, frames = 8;
  const auto in = distinct_inputs(channels, frames);
  const auto out = run<MonoGain>(channels, frames, in);

  REQUIRE(out.size() == std::size_t(channels));
  for(int c = 0; c < channels; c++)
    for(int i = 0; i < frames; i++)
      REQUIRE(out[c][i] == Catch::Approx(in[c][i])); // gain init == 1

  // The failure mode: channel 1 coming out as a copy of channel 0.
  REQUIRE(out[1][frames - 1] != Catch::Approx(out[0][frames - 1]));
}

TEST_CASE("per-channel state is not shared between channels", "[polyphony]")
{
  constexpr int channels = 2, frames = 8;
  const auto in = distinct_inputs(channels, frames);
  const auto out = run<MonoAccumulator>(channels, frames, in);

  REQUIRE(out.size() == std::size_t(channels));

  // Each channel accumulates only its own input.
  for(int c = 0; c < channels; c++)
  {
    float sum = 0.f;
    for(int i = 0; i < frames; i++)
    {
      sum += in[c][i];
      REQUIRE(out[c][i] == Catch::Approx(sum));
    }
  }
}
