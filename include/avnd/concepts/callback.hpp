#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/function_reflection.hpp>
#include <avnd/concepts/generic.hpp>

// clang-format: off

// TODO https://github.com/SuperV1234/Experiments/blob/master/function_ref.cpp
namespace avnd
{
// Used to store callbacks without allocating.
// By convention, the first argument to T::function must be T::context.
template <typename T>
concept function_view_ish =
  requires(T t) {
    { T::function } -> function;
    { T::context } -> pointer;
};

template <typename T>
concept view_callback =
    function_view_ish<std::decay_t<decltype(T{}.call)>>;

// Used to store std::function and similar types which handle their
// allocations on their own
template <typename T>
concept dynamic_callback =
    function_ish<std::decay_t<decltype(T{}.call)>>
 && !view_callback<T>;

template <typename T>
concept callback = dynamic_callback<T> || view_callback<T>;
}

// clang-format: on
