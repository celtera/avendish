#pragma once
#include <helpers/static_string.hpp>
#include <string_view>

namespace avnd
{
template <static_string lit, typename FP>
struct audio_sample
{
  static consteval auto name() { return std::string_view{lit.value}; }

  FP sample;
};

template <static_string lit, typename FP>
struct audio_channel
{
  static consteval auto name() { return std::string_view{lit.value}; }

  FP* channel{};
};

template <static_string lit, typename FP, int WantedChannels>
struct fixed_audio_bus
{
  static consteval auto name() { return std::string_view{lit.value}; }
  static constexpr int channels() { return WantedChannels; }

  FP** samples{};
};

template <static_string lit, typename FP>
struct dynamic_audio_bus
{
  static consteval auto name() { return std::string_view{lit.value}; }

  FP** samples{};
  int channels{};
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
