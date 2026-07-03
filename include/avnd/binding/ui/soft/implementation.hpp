#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Include this header in exactly ONE translation unit of the final binary:
// it emits the Nuklear and canvas_ity implementations with the same
// configuration the soft UI headers were compiled against.

#define NK_IMPLEMENTATION
#include <avnd/binding/ui/soft/nk_config.hpp>
#undef NK_IMPLEMENTATION

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244 4305)
#endif

#define CANVAS_ITY_IMPLEMENTATION
#include <canvas_ity.hpp>
#undef CANVAS_ITY_IMPLEMENTATION

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
