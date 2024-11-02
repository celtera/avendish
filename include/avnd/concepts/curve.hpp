#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/enums.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{

template <typename T>
concept curve_segment_start_end = requires(T t) {
                                    t.start.x;
                                    t.start.y;
                                    t.end.x;
                                    t.end.y;
                                  };

template <typename T>
concept curve_segment_float = requires(T t) {
                                t.start = 0.f;
                                t.end = 0.f;
                              };

template <typename T>
concept curve_segment = curve_segment_start_end<T> || curve_segment_float<T>;

template <typename T>
concept curve_segment_map_gamma
    = requires(T t) { t.gamma = 0.f; } || requires(T t) { t.power = 0.f; };

struct dummy_function_for_curve
{
  int foo = 123;
  void operator()(auto x) { return foo * x; }
};

template <typename T>
concept curve_segment_map_function = requires(T t) {
  t.function = dummy_function_for_curve{};
} || requires(T t) { t.map = dummy_function_for_curve{}; };

template <typename T>
concept curve_segment_map = curve_segment_map_gamma<T> || curve_segment_map_function<T>;

template <typename T>
concept mapped_curve_segment = curve_segment<T> && curve_segment_map<T>;

template <typename T>
concept curve = requires(T t) {
                  {
                    t[0]
                    } -> curve_segment;
                };

template <typename T>
concept curve_port = requires(T t) {
                       {
                         t.value
                         } -> curve;
                     };

}
