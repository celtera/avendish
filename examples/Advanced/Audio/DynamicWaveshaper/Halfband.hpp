#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Adapted from score-addon-synthimi/Dsp/Halfband.hpp. Polyphase allpass
 * halfbands follow Valenzuela and Constantinides, as in Laurent de Soras'
 * HIIR (WTFPL). These are HIIR's order-21 elliptic coefficients for a 0.04
 * transition width at the higher rate. Both directions use all ten
 * coefficients: unlike a short synthesizer interpolator, this accepts
 * arbitrary effect input. Passband ends at 0.46 of the host rate; stopband
 * begins at 0.54. No coefficient design or allocation happens at runtime.
 */

#include <array>
#include <cmath>
#include <cstddef>

namespace ao::dynamic_waveshaper::halfband
{
struct Chains
{
  static constexpr std::array<float, 10> coefficients{
      0.026872581726589895f, 0.10211979750762745f,
      0.21186457986634402f, 0.33896530820005222f,
      0.46822765324642385f, 0.58941249499690829f,
      0.69777813051993742f, 0.7931312528855613f,
      0.87850905287240721f, 0.95921252955742053f};
  std::array<float, 10> x{}, y{};

  void reset() noexcept
  {
    x.fill(0.f);
    y.fill(0.f);
  }

  void process(float& even, float& odd) noexcept
  {
    for(std::size_t i = 0; i < coefficients.size(); i += 2)
    {
      const float a = (even - y[i]) * coefficients[i] + x[i];
      x[i] = even;
      y[i] = std::abs(a) < 1.e-20f ? 0.f : a;
      even = a;
      const float b = (odd - y[i + 1]) * coefficients[i + 1] + x[i + 1];
      x[i + 1] = odd;
      y[i + 1] = std::abs(b) < 1.e-20f ? 0.f : b;
      odd = b;
    }
  }
};

struct Interpolator2x
{
  Chains chains;
  void prepare() noexcept { reset(); }
  void reset() noexcept { chains.reset(); }
  void process(float input, float& first, float& second) noexcept
  {
    first = second = input;
    chains.process(first, second);
  }
};

struct Decimator2x
{
  Chains chains;
  void prepare() noexcept { reset(); }
  void reset() noexcept { chains.reset(); }
  float process(float first, float second) noexcept
  {
    // Opposite phase ordering to the interpolator; first is earlier.
    chains.process(second, first);
    return 0.5f * (first + second);
  }
};
}
