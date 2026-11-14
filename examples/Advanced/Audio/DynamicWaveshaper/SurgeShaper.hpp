#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cstddef>

namespace ao::dynamic_waveshaper
{
inline constexpr int algorithm_count = 43;

const char* algorithmName(int algorithm) noexcept;

// Fixed inline storage keeps SST's SIMD types, macros and tables out of all
// host/UI translation units. Its pinned size/alignment is checked in the .cpp.
class Shaper
{
public:
  Shaper() noexcept;
  ~Shaper() noexcept;
  Shaper(const Shaper&) noexcept;
  Shaper& operator=(const Shaper&) noexcept;
  void reset(int algorithm) noexcept;
  float process(float input, float drive) noexcept;

private:
  struct State;
  State& state() noexcept;
  const State& state() const noexcept;
  alignas(16) std::byte storage_[96];
};
}
