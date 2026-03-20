#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/enums.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{
/**
 * @brief timestamp tag: used to state that 
 * an element is a timestamp, when there could be an ambiguity.
 * 
 * Used for instance in messages and callbacks, to indicate that 
 * the first int64_t element being passed is the timestamp and not 
 * a value passed in the host environment.
 * 
 * e.g.
 * ```
 * struct { 
 *   void operator()(int64_t, float); 
 * } msg1;
 * struct { 
 *   enum { timestamp; };
 *   void operator()(int64_t ts, float x); 
 * } msg2;
 * ```
 * 
 * Here msg1 will exepect an input of an int and a float, msg2 will expect only a float
 * and the binding code will pass the timestamp as first argument.
 */
AVND_DEFINE_TAG(timestamp)
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
