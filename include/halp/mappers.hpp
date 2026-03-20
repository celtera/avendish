#pragma once

#include <cmath>
#include <halp/modules.hpp>

#include <ratio>

HALP_MODULE_EXPORT
namespace halp
{

// FIXME: when clang supports double arguments we can use them here instead
template <typename Ratio>
struct log_mapper
{
  // Formula from http://benholmes.co.uk/posts/2017/11/logarithmic-potentiometer-laws
  static constexpr double mid = double(Ratio::num) / double(Ratio::den);
  static constexpr double b = (1. / mid - 1.) * (1. / mid - 1.);
  static constexpr double a = 1. / (b - 1.);
  static double map(double v) noexcept { return a * (std::pow(b, v) - 1.); }
  static double unmap(double v) noexcept { return std::log(v / a + 1.) / std::log(b); }
};

template <int Power>
struct pow_mapper
{
  static double map(double v) noexcept { return std::pow(v, (double)Power); }
  static double unmap(double v) noexcept { return std::pow(v, 1. / Power); }
};

template <typename T>
struct inverse_mapper
{
  static double map(double v) noexcept { return T::unmap(v); }
  static double unmap(double v) noexcept { return T::map(v); }
};

}
