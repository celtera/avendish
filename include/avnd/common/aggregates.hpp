#pragma once
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/aggregates.base.hpp>

#if AVND_USE_BOOST_PFR
// MSVC / clang < 21 / gcc < 16: boost.pfr (with its recursive flattening).
#include <avnd/common/aggregates.pfr.hpp>
#else
// clang >= 21 / gcc >= 16 (with or without -freflection): a structured-binding
// flattener that natively handles recursive_group + raw port arrays, with no
// std::meta dependency (so it is identical whether or not reflection is on, and
// works on members of types nested in class templates).
#include <avnd/common/aggregates.structured.hpp>
#endif

// All backends expose the flattened avnd::pfr::* interface (recursive groups and
// port arrays are inlined into the leaf list), so the introspection layer always
// goes through it rather than raw structured-binding access.
#define AVND_PFR_FLATTEN 1
