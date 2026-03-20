#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>
#include <halp/controls.hpp>
#include <halp/modules.hpp>

#include <map>

HALP_MODULE_EXPORT
namespace halp
{

template <typename T>
struct sample_accurate_values
{
  boost::container::small_flat_map<int, T, 16> values;
};

template <typename T>
struct accurate
    : T
    , sample_accurate_values<std::decay_t<decltype(T::value)>>
{
};

}
