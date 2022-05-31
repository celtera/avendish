#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/span_polyfill.hpp>

#include <halp/static_string.hpp>
#include <halp/inline.hpp>

#include <cstdint>

#include <string_view>
namespace halp
{
template <static_string Name, typename FP, static_string Desc = "">
struct audio_sample
{
  static consteval auto name() { return std::string_view{Name.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }

  FP sample;

  HALP_INLINE_FLATTEN operator FP&() noexcept { return sample; }
  HALP_INLINE_FLATTEN operator FP() const noexcept { return sample; }
  HALP_INLINE_FLATTEN auto& operator=(FP t) noexcept
  {
    sample = t;
    return *this;
  }
};

template <static_string Name, typename FP, static_string Desc = "">
struct audio_channel
{
  static consteval auto name() { return std::string_view{Name.value}; }

  FP* channel;

  HALP_INLINE_FLATTEN operator FP*() const noexcept { return channel; }
  HALP_INLINE_FLATTEN FP& operator[](std::size_t i) noexcept { return channel[i]; }
  HALP_INLINE_FLATTEN FP operator[](std::size_t i) const noexcept { return channel[i]; }
};



template <static_string lit, typename FP, int WantedChannels, static_string Desc = "">
struct fixed_audio_bus
{
  static consteval auto name() { return std::string_view{lit.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }
  static constexpr int channels() { return WantedChannels; }

  FP** samples{};
  HALP_INLINE_FLATTEN operator FP**() const noexcept { return samples; }
  HALP_INLINE_FLATTEN FP* operator[](std::size_t i) const noexcept { return samples[i]; }
  HALP_INLINE_FLATTEN avnd::span<FP> channel(std::size_t i, std::size_t frames) const noexcept
  {
    return {samples[i], frames};
  }
};

template <static_string Name, typename FP, static_string Desc = "">
struct dynamic_audio_bus
{
  static consteval auto name() { return std::string_view{Name.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }

  FP** samples{};
  int channels{};

  HALP_INLINE_FLATTEN operator FP**() const noexcept { return samples; }
  HALP_INLINE_FLATTEN FP* operator[](std::size_t i) const noexcept { return samples[i]; }
  HALP_INLINE_FLATTEN avnd::span<FP> channel(std::size_t i, std::size_t frames) const noexcept
  {
    return {samples[i], frames};
  }
};


template <static_string Name, typename FP, static_string Desc = "">
struct audio_spectrum_channel
{
  static consteval auto name() { return std::string_view{Name.value}; }

  FP* channel{};

  struct {
    FP* amplitude{};
    FP* phase{};
  } spectrum;

  HALP_INLINE_FLATTEN operator FP*() const noexcept { return channel; }
  HALP_INLINE_FLATTEN FP& operator[](std::size_t i) noexcept { return channel[i]; }
  HALP_INLINE_FLATTEN FP operator[](std::size_t i) const noexcept { return channel[i]; }
};

template <static_string lit, typename FP, int WantedChannels, static_string Desc = "">
struct fixed_audio_spectrum_bus
{
    static consteval auto name() { return std::string_view{lit.value}; }
    static consteval auto description() { return std::string_view{Desc.value}; }
    static constexpr int channels() { return WantedChannels; }

    FP** samples{};

    struct {
      FP** amplitude{};
      FP** phase{};
    } spectrum;

    HALP_INLINE_FLATTEN operator FP**() const noexcept { return samples; }
    HALP_INLINE_FLATTEN FP* operator[](std::size_t i) const noexcept { return samples[i]; }
    HALP_INLINE_FLATTEN avnd::span<FP> channel(std::size_t i, std::size_t frames) const noexcept
    {
        return {samples[i], frames};
    }
};

template <static_string Name, typename FP, static_string Desc = "">
struct dynamic_audio_spectrum_bus
{
  static consteval auto name() { return std::string_view{Name.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }

  FP** samples{};
  struct {
    FP** amplitude{};
    FP** phase{};
  } spectrum;

  int channels{};

  HALP_INLINE_FLATTEN operator FP**() const noexcept { return samples; }
  HALP_INLINE_FLATTEN FP* operator[](std::size_t i) const noexcept { return samples[i]; }
  HALP_INLINE_FLATTEN avnd::span<FP> channel(std::size_t i, std::size_t frames) const noexcept
  {
      return {samples[i], frames};
  }
};

struct tick
{
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

namespace halp
{
template <static_string lit, typename FP, static_string Desc = "">
struct audio_bus
{
  static consteval auto name() { return std::string_view{lit.value}; }
  static consteval auto description() { return std::string_view{Desc.value}; }

  FP** samples{};
  int channels{};

  HALP_INLINE_FLATTEN operator avnd::span<FP*>() const noexcept { return {samples, channels}; }
  HALP_INLINE_FLATTEN avnd::span<FP> channel(std::size_t i, std::size_t frames) const noexcept
  {
    return {samples[i], frames};
  }
};

template <static_string lit>
using audio_input_bus = audio_bus<lit, const double>;
template <static_string lit>
using audio_output_bus = audio_bus<lit, double>;
}

namespace halp
{

template <typename T>
struct sample_type_from_channels_t;

template <typename T>
struct sample_type_from_channels_t<T**>
{
  using value_type = T;
};

template <typename T>
struct pointer_to_member_reflection_t;

template <typename T, typename C>
struct pointer_to_member_reflection_t<T C::*>
{
  using member_type = T;
};

template <auto memfun>
using pointer_to_member_type =
    typename pointer_to_member_reflection_t<decltype(memfun)>::member_type;

// Given &inputs::audio, gives us the type of the "samples" member variable of audio
template <auto memfun>
using sample_type_from_channels
    = std::remove_const_t<typename sample_type_from_channels_t<
        std::decay_t<decltype(pointer_to_member_type<memfun>{}.samples)>>::value_type>;

template <static_string lit, auto mimicked_channel>
struct mimic_audio_bus
    : public dynamic_audio_bus<lit, sample_type_from_channels<mimicked_channel>>
{
  static constexpr auto mimick_channel = mimicked_channel;
};
}
