#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/index_sequence.hpp>
#include <avnd/common/inline.hpp>
#include <avnd/common/tuple.hpp>

#include <string_view>

namespace avnd
{
template <typename...>
struct typelist
{
};
}

// Selects the aggregate backend. AVND_USE_BOOST_PFR can be pre-defined to force
// it (0 = structured-binding / P1061, 1 = boost.pfr); otherwise it is detected.
//
// The structured-binding backend needs only P1061 (structured-binding packs) and
// pack indexing — both available on clang >= 21 and gcc >= 16 *including in C++23
// mode* (only __cplusplus is gated to C++26, the feature macros are not). So the
// selection keys on the feature macros, NOT __cplusplus, which lets the modern
// backend be used at -std=c++23 as well as -std=c++26.
//
// clang-format off
#if !defined(AVND_USE_BOOST_PFR)
  #if defined(__clang_major__) && (__clang_major__ < 21)
    #define AVND_USE_BOOST_PFR 1   // clang < 21: no P1061 packs
  #elif defined(_MSC_VER) && (!defined(__clang_major__) || (__clang_major__ < 21))
    #define AVND_USE_BOOST_PFR 1   // MSVC (cl, or clang-cl < 21)
  #elif !defined(__cpp_structured_bindings) || (__cpp_structured_bindings < 202403L) \
        || !defined(__cpp_pack_indexing) || (__cpp_pack_indexing < 202311L)
    #define AVND_USE_BOOST_PFR 1   // no P1061 / pack indexing (e.g. gcc < 16)
  #else
    #define AVND_USE_BOOST_PFR 0   // clang >= 21 / gcc >= 16: structured bindings
  #endif
#endif
// clang-format on
