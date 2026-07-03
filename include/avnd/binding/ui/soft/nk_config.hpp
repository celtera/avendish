#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Single place defining the Nuklear configuration for the soft UI backend,
// so every translation unit sees the same nuklear.h ABI. The implementation
// (NK_IMPLEMENTATION) is emitted by soft/implementation.hpp in exactly one TU.
//
// No font baking, no vertex output: text is measured and rasterized by
// canvas_ity through avnd::soft_ui::font_registry, so nk_user_font only
// carries a width callback.

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4116 4996)
#endif

#include <nuklear.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
