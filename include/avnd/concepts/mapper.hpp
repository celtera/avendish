#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>

#include <type_traits>

namespace avnd
{
/**
 * Used to define how UI sliders behave.
 */
template <typename T>
concept mapper = requires(T t) {
                   // From linear to eased domain
                   {
                     t.map(0.)
                     } -> std::convertible_to<double>;
                   // From eased domain to linear domain
                   {
                     t.unmap(0.)
                     } -> std::convertible_to<double>;
                 };

template <typename C>
concept has_mapper = requires { C::mapper(); } || requires { sizeof(C::mapper); }
                     || requires { sizeof(typename C::mapper); };

}
