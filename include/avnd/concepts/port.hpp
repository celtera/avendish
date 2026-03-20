#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/aggregates.hpp>
#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/tag.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{

type_or_value_qualification(inputs)
type_or_value_reflection(inputs)

type_or_value_qualification(outputs)
type_or_value_reflection(outputs)

// For symbol / selector-based environments:
// The first argument will be the symbol
// and it has to be std::string-like
AVND_DEFINE_TAG(dynamic_symbol)
}
