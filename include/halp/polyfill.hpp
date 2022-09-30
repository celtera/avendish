#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#if defined(__APPLE__) || (defined(__clang__) && __clang_major__ <= 13)
#define clang_buggy_consteval constexpr
#else
#define clang_buggy_consteval consteval
#endif
