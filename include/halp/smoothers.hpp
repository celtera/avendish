#pragma once

#include <cmath>

namespace halp
{
static const constexpr double pi = 3.141592653589793238462643383279502884;

// A basic smooth specification
template <int T>
struct milliseconds_smooth
{
  static constexpr float milliseconds{T};

  // Alternative:
  // static constexpr std::chrono::milliseconds duration{T};
};

// Same thing but explicit control over the smoothing
// ratio
template <int T>
struct exp_smooth
{
  static constexpr auto ratio(double sample_rate) noexcept
  {
    return std::exp(-2. * pi / (T * 1e-3 * sample_rate));
  }
};

}
