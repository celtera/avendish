#pragma once
#include <avnd/helpers/static_string.hpp>
#include <string_view>

namespace avnd
{
template <static_string lit, typename FP>
struct audio_sample
{
  static consteval auto name() { return std::string_view{lit.value}; }

  FP sample;

  operator FP&() noexcept { return sample; }
  operator FP() const noexcept { return sample; }
  auto& operator=(FP t) noexcept { sample = t; return *this; }
};

template <static_string lit, typename FP>
struct audio_channel
{
  static consteval auto name() { return std::string_view{lit.value}; }

  FP* channel{};

  operator FP*() const noexcept { return channel; }
  FP& operator[](std::size_t i) noexcept { return channel[i]; }
  FP operator[](std::size_t i) const noexcept { return channel[i]; }
};

template <static_string lit, typename FP, int WantedChannels>
struct fixed_audio_bus
{
  static consteval auto name() { return std::string_view{lit.value}; }
  static constexpr int channels() { return WantedChannels; }

  FP** samples{};
  operator FP**() const noexcept { return samples; }
  FP* operator[](std::size_t i) const noexcept { return samples[i]; }
};

template <static_string lit, typename FP>
struct dynamic_audio_bus
{
  static consteval auto name() { return std::string_view{lit.value}; }

  FP** samples{};
  int channels{};

  operator FP**() const noexcept { return samples; }
  FP* operator[](std::size_t i) const noexcept { return samples[i]; }
};

struct tick {
  int frames{};
};

struct setup
{
  int input_channels{};
  int output_channels{};
  int frames{};
  double rate{};
};
}
