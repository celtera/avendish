#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/concepts/generic.hpp>
#include <common/concepts_polyfill.hpp>
#include <common/function_reflection.hpp>

#include <string_view>

namespace avnd
{

// TODO https://github.com/SuperV1234/Experiments/blob/master/function_ref.cpp

template <typename T>
concept function_view_ish = requires(T t)
{
  {
    T::function
    } -> function;
  {
    T::context
    } -> pointer;
};

template <typename T>
concept callback = requires(T t)
{
  {
    t.name()
    } -> string_ish;
  {
    t.call
    } -> function_ish;
};

}
