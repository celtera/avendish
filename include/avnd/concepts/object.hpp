#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/concepts/generic.hpp>
#include <avnd/common/concepts_polyfill.hpp>

namespace avnd
{

template <typename T>
concept can_initialize = requires(T t)
{
  &T::initialize;
};

}
