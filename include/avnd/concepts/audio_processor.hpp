#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/enums.hpp>

#include <iterator>

namespace avnd
{
template <typename T>
concept has_programs = requires(T t) { std::size(T::programs); };

template <typename T>
concept can_bypass = requires(T t) { t.bypass; };

template <typename T>
concept can_prepare = requires(T t) { t.prepare({}); };

/**
 * This tag indicates that a processor is to be treated 
 * as supporting CV-like control for its I/O, even if it looks like an 
 * audio processor.
 */
AVND_DEFINE_TAG(cv)

/**
 * This tag indicates that an object is stateless, e.g. 
 * the same object instance can be reused across multiple invocations
 * for separate input values ; the output only depends 
 * on the inputs and nothing else.
 */
AVND_DEFINE_TAG(stateless)
}
