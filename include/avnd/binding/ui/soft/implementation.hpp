#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Include this header in exactly ONE translation unit of the final binary:
// it emits the Nuklear and canvas_ity implementations.
//
// Both libraries keep their implementation sections outside the header
// guard, so the raw headers are included here directly -- going through
// nk_config.hpp would be a no-op when the declarations were already pulled
// in earlier in this TU (#pragma once). The configuration macros must match
// nk_config.hpp; identical redefinition is harmless when they are already
// set.

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4116 4244 4305 4996)
#endif

#define NK_IMPLEMENTATION
#include <nuklear.h>
#undef NK_IMPLEMENTATION

#define CANVAS_ITY_IMPLEMENTATION
#include <canvas_ity.hpp>
#undef CANVAS_ITY_IMPLEMENTATION

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
