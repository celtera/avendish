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

// clang-format off
#if defined(__clang_major__) && (__clang_major__ < 21)
#define AVND_USE_BOOST_PFR 1
#endif

#if defined(_MSC_VER)
#if (!defined(__clang_major__) || (__clang_major__ < 21))
#define AVND_USE_BOOST_PFR 1
#endif
#endif

#if !defined(AVND_USE_BOOST_PFR)
#if (__cpp_structured_bindings < 202403L) || (__cpp_pack_indexing < 202311L) || (__cplusplus < 202400L)
#define AVND_USE_BOOST_PFR 1
#endif
#endif
// clang-format on
