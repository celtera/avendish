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

// Selects the aggregate backend (0 = structured-binding/P1061, 1 = boost.pfr).
// Keys on the P1061 / pack-indexing feature macros, not __cplusplus.
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
