/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "SurgeShaper.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <memory>
#include <new>
#include <string>

// sst/utilities/globals.h includes <math.h>, then SIMDe, which takes a
// defined HUGE_VAL to mean that <cmath> is there and calls std::isnan: the
// <math.h> of MSVC does not declare it.
#include <cmath>
#include <sst/waveshapers.h>

namespace ao::dynamic_waveshaper
{
namespace
{
using namespace sst::waveshapers;
static_assert(int(WaveshaperType::n_ws_types) == algorithm_count,
              "Build the pinned Surge XT catalog with SURGE_XT_1X_WST=1");

bool finite(float x) noexcept
{
  // Remains a real check in hosts built with -ffast-math.
  return (std::bit_cast<std::uint32_t>(x) & 0x7f800000u) != 0x7f800000u;
}

struct Catalog
{
  std::array<QuadWaveshaperPtr, algorithm_count> kernels{};
  std::array<QuadWaveshaperState, algorithm_count> silence{};
  float digitalOffset{};

  Catalog() noexcept
  {
    const auto zero = SIMD_MM(setzero_ps)();
    const auto one = SIMD_MM(set1_ps)(1.f);
    for(int i = 1; i < algorithm_count; ++i)
    {
      const auto type = static_cast<WaveshaperType>(i);
      auto& state = silence[i];
      float registers[n_waveshaper_registers];
      initializeWaveshaperRegister(type, registers);
      for(int r = 0; r < n_waveshaper_registers; ++r)
        state.R[r] = SIMD_MM(set1_ps)(registers[r]);
      state.init = SIMD_MM(cmpeq_ps)(zero, zero);
      auto kernel = kernels[i] = GetQuadWaveshaper(type);

      // Initialize every lazy table, in enum order. In particular, Fuzz and
      // Fuzz Soft Clip share a generator but have separate tables. Never let
      // instance creation or audio-thread shape changes decide their order.
      kernel(&state, zero, one);
      const auto resting = kernel(&state, zero, one);
      if(type == WaveshaperType::wst_digital)
        digitalOffset = SIMD_MM(cvtss_f32)(resting);

      // At zero input these DC blockers have constant x[-1] and y[-1]=0.
      // The two evaluations above establish x[-1] (and ADAA history), so
      // obtain the exact equilibrium without a long warmup on startup.
      switch(type)
      {
        case WaveshaperType::wst_cheby2:
        case WaveshaperType::wst_cheby3:
        case WaveshaperType::wst_cheby4:
        case WaveshaperType::wst_cheby5:
        case WaveshaperType::wst_add12:
        case WaveshaperType::wst_add14:
        case WaveshaperType::wst_add12345:
        case WaveshaperType::wst_addsaw3:
        case WaveshaperType::wst_fuzz:
        case WaveshaperType::wst_fuzzsoft:
        case WaveshaperType::wst_fuzzheavy:
        case WaveshaperType::wst_fuzzctr:
        case WaveshaperType::wst_fuzzsoftedge:
          state.R[1] = zero;
          break;
        case WaveshaperType::wst_softrect:
          state.R[3] = zero;
          break;
        default:
          break;
      }
    }
  }
};

// Eager module initialization: neither prepare(), reset(), nor process()
// has a guarded local static, a table allocation, or an initialization lock.
const Catalog catalog;
}

struct Shaper::State
{
  sst::waveshapers::QuadWaveshaperState registers{};
  sst::waveshapers::QuadWaveshaperPtr kernel{};
  int algorithm{};
};

Shaper::State& Shaper::state() noexcept
{
  return *std::launder(reinterpret_cast<State*>(storage_));
}

const Shaper::State& Shaper::state() const noexcept
{
  return *std::launder(reinterpret_cast<const State*>(storage_));
}

Shaper::Shaper() noexcept
{
  static_assert(sizeof(State) <= sizeof(storage_));
  static_assert(alignof(State) <= alignof(Shaper));
  ::new(static_cast<void*>(storage_)) State{};
}

Shaper::~Shaper() noexcept { std::destroy_at(&state()); }

Shaper::Shaper(const Shaper& other) noexcept
{
  ::new(static_cast<void*>(storage_)) State{other.state()};
}

Shaper& Shaper::operator=(const Shaper& other) noexcept
{
  state() = other.state();
  return *this;
}

const char* algorithmName(int algorithm) noexcept
{
  return sst::waveshapers::wst_names[
      algorithm >= 0 && algorithm < algorithm_count ? algorithm : 0];
}

void Shaper::reset(int algorithm) noexcept
{
  auto& s = state();
  s.algorithm = algorithm >= 0 && algorithm < algorithm_count ? algorithm : 0;
  s.registers = catalog.silence[s.algorithm];
  s.kernel = catalog.kernels[s.algorithm];
}

float Shaper::process(float input, float drive) noexcept
{
  auto& s = state();
  if(!finite(input))
  {
    reset(s.algorithm);
    return 0.f;
  }
  input = std::clamp(input, -64.f, 64.f);
  if(!s.kernel)
    return input;
  drive = finite(drive) ? std::clamp(drive, 0.063095734f, 63.095734f) : 1.f;

  const auto result = s.kernel(
      &s.registers, SIMD_MM(set1_ps)(input), SIMD_MM(set1_ps)(drive));
  float output = SIMD_MM(cvtss_f32)(result);
  // Digital quantizes around a half-step and maps silence to -drive/32.
  // Like Surge's DistortionEffect, center that resting output; do not apply
  // the ordinary inverse-drive normalization, which destabilizes Digital.
  if(s.algorithm == int(sst::waveshapers::WaveshaperType::wst_digital))
    output -= catalog.digitalOffset * drive;
  if(!finite(output))
  {
    reset(s.algorithm);
    return 0.f;
  }
  return output;
}
}
