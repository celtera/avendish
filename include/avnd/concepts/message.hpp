#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/concepts/generic.hpp>
#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/function_reflection.hpp>

#include <string_view>

namespace avnd
{

template <typename T>
concept message = requires(T t)
{
  {
    t.name()
    } -> string_ish;
  typename avnd::function_reflection<T::func()>::return_type;
};

type_or_value_qualification(messages) type_or_value_reflection(messages)

}
