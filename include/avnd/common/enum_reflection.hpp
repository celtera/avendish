#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Routes the avnd enum-reflection API to the P2996 or magic_enum backend.

#include <avnd/common/meta_polyfill.hpp>

#if AVND_USE_STD_REFLECTION
#include <avnd/common/enum_reflection.p2996.hpp>
#else
#include <avnd/common/enum_reflection.magic_enum.hpp>
#endif
