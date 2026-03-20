#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{

template <typename T>
concept can_initialize = requires(T t) { &T::initialize; };

/**
 * @brief deprecated tag: used when an object should not be shown 
 * in lists, etc... to the users, e.g. when it's an old version
 * that must be updated.
 */
AVND_DEFINE_TAG(deprecated)
}
