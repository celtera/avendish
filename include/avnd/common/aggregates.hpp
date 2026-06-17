#pragma once
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/aggregates.base.hpp>
#include <avnd/common/meta_polyfill.hpp>

#if AVND_USE_STD_REFLECTION
#include <avnd/common/aggregates.p2996.hpp>
#elif AVND_USE_BOOST_PFR
#include <avnd/common/aggregates.pfr.hpp>
#else
#include <avnd/common/aggregates.p1061.hpp>
#endif
