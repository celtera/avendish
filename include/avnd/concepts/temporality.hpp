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

/**
 * @brief temporal tag: the object has a notion of time
 * and can be displayed as-is in a timeline.
 */
AVND_DEFINE_TAG(temporal)

/**
 * @brief loops_by_default tag: the object has a temporal content 
 * for which it makes more sense to start in a looping state. 
 * e.g. a drum loop etc
 */
AVND_DEFINE_TAG(loops_by_default)
}
