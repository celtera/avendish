#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <iterator>

namespace avnd
{
template <typename T>
concept has_programs = requires(T t)
{
  std::size(T::programs);
};

template <typename T>
concept can_bypass = requires(T t)
{
  t.bypass;
};

template <typename T>
concept can_prepare = requires(T t)
{
  t.prepare({});
};

}
