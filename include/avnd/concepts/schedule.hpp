#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>

#include <type_traits>

namespace avnd
{
/**
 * "schedule" concept: used to give access to a schedule / defer_low-like API
 */
template <typename C>
concept has_schedule =
    requires { C::schedule(); }
 || requires { sizeof(C::schedule); }
 || requires { sizeof(typename C::schedule); };
}
