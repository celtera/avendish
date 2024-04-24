#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/enums.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{
/**
 * @brief single_exec tag: the 'process' method should only be called once 
 * per execution.
 * 
 * Example: in ossia, this means that the processor can be assigned to a state.
 */
AVND_DEFINE_TAG(single_exec)

/**
 * @brief process_exec tag: start() and stop() methods will be called.
 */
AVND_DEFINE_TAG(process_exec)
}
