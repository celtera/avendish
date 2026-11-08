#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/modules.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>

HALP_MODULE_EXPORT
namespace halp
{
// NaN and infinity tests that hold under -ffast-math (-ffinite-math-only),
// where the compiler takes every double to be finite and folds std::isnan,
// std::isfinite, and even tests of the bits read through a cast: these read
// the exponent bits through an empty asm statement, which hides where they
// come from.
namespace detail
{
inline std::uint64_t opaque_bits(double v) noexcept
{
  std::uint64_t bits;
  std::memcpy(&bits, &v, sizeof(bits));
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__EMSCRIPTEN__)
  asm("" : "+r"(bits));
#endif
  return bits;
}
}

inline bool safe_isfinite(double v) noexcept
{
#if defined(_MSC_VER) || defined(__EMSCRIPTEN__) || !__FINITE_MATH_ONLY__
  return std::isfinite(v);
#else
  return (detail::opaque_bits(v) & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
#endif
}
inline bool safe_isnan(double v) noexcept
{
#if defined(_MSC_VER) || defined(__EMSCRIPTEN__) || !__FINITE_MATH_ONLY__
  return std::isnan(v);
#else
  return (detail::opaque_bits(v) & 0x7fffffffffffffffull) > 0x7ff0000000000000ull;
#endif
}
inline bool safe_isinf(double v) noexcept
{
#if defined(_MSC_VER) || defined(__EMSCRIPTEN__) || !__FINITE_MATH_ONLY__
  return std::isinf(v);
#else
  return (detail::opaque_bits(v) & 0x7fffffffffffffffull) == 0x7ff0000000000000ull;
#endif
}
}
