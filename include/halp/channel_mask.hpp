#pragma once
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace halp
{
// Which of the 16 MIDI channels are on: value[0] is channel 1. Init is a bit
// mask, bit 0 for channel 1. Hosts draw a toggle per channel.
template <static_string lit, uint16_t Init = 0xFFFF>
struct channel_mask
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
  enum class widget
  {
    channel_mask
  };

  static constexpr std::array<bool, 16> init() noexcept
  {
    std::array<bool, 16> res{};
    for(int i = 0; i < 16; i++)
      res[i] = (Init >> i) & 1;
    return res;
  }

  std::array<bool, 16> value = init();

  //! The channels that are on, as a bit mask.
  constexpr uint16_t mask() const noexcept
  {
    uint16_t res = 0;
    for(int i = 0; i < 16; i++)
      if(value[i])
        res |= uint16_t(1u << i);
    return res;
  }
};
}
