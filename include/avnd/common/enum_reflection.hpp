#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Routes the avnd enum-reflection API (avnd::enum_count / enum_name / enum_names /
// enum_values / enum_entries / enum_cast / enum_underlying) to either a C++26
// static-reflection backend or the magic_enum fallback, depending on the compiler.
//
// See enum_reflection.p2996.hpp and enum_reflection.magic_enum.hpp.

#include <avnd/common/meta_polyfill.hpp>

#if AVND_USE_STD_REFLECTION
#include <avnd/common/enum_reflection.p2996.hpp>
#else
#include <avnd/common/enum_reflection.magic_enum.hpp>
#endif
