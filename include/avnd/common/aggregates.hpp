#pragma once
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/aggregates.base.hpp>

#if AVND_USE_BOOST_PFR
#include <avnd/common/aggregates.pfr.hpp>
#else
#include <avnd/common/aggregates.p1061.hpp>
#endif
