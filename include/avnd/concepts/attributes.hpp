#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/enums.hpp>

namespace avnd
{

/**
 * This tag indicates that a specific input is to
 * be considered as an attribute in environments in which
 * this makes sense.
 *
 * Usage:
 *
 * ```
 * struct {
 *   static consteval auto name() { return "foobar"; }
 *   enum { class_attribute };
 *   int value;
 * } foorbar_port;
 * ```
 *
 * See also halp/attributes.hpp
 */
AVND_DEFINE_TAG(class_attribute)

template <typename T>
concept attribute_port = tag_class_attribute<T>;
}
