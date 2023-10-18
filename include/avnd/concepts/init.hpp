#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/enums.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{
/**
 * @brief skip_init tag: will skip explicit initialization of 
 * controls.
 * 
 * Useful when the objects are constructed through an explicit ctor - see 
 * the ump objects for instance.
 * 
 * Note that update() will still be called.
 */
AVND_DEFINE_TAG(skip_init)
}
