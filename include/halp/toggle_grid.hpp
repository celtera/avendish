#pragma once
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace halp
{
// A grid of named toggles, one per choice of a set: value[i] is whether the
// choice labels()[i] is on. A field derived from it names the cells with
//   static constexpr std::array<std::string_view, N> labels();
// and may set how many cells make a row with
//   static constexpr int items_per_line();
// Init is a bit mask, bit i for cell i. Hosts draw a toggle per cell.
template <static_string lit, std::size_t N, uint64_t Init = 0>
struct toggle_grid
{
  static_assert(N > 0 && N <= 64);
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
  enum class widget
  {
    toggle_grid
  };
  static constexpr std::size_t count = N;

  static constexpr std::array<bool, N> init() noexcept
  {
    std::array<bool, N> res{};
    for(std::size_t i = 0; i < N; i++)
      res[i] = (Init >> i) & 1;
    return res;
  }

  std::array<bool, N> value = init();

  //! The cells that are on, as a bit mask.
  constexpr uint64_t mask() const noexcept
  {
    uint64_t res = 0;
    for(std::size_t i = 0; i < N; i++)
      if(value[i])
        res |= uint64_t(1) << i;
    return res;
  }
};
}
