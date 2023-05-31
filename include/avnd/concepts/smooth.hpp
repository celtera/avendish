#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <type_traits>
#include <avnd/common/concepts_polyfill.hpp>

namespace avnd
{
template <typename C>
concept has_smooth = requires { C::smooth(); } || requires { sizeof(C::smooth); }
                     || requires { sizeof(typename C::smooth); };
}
