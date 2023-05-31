#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>

#include <type_traits>

namespace avnd
{
template <typename C>
concept has_range = requires { C::range(); } || requires { sizeof(C::range); }
                    || requires { sizeof(typename C::range); };
}
