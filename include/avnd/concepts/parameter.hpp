#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/wrappers/widgets.hpp>

#include <string>

namespace avnd
{
/**
 * A "parameter" port is something that has a value:
 *
 * struct {
 *   float value;
 * };
 */

// Concepts related to inputs / outputs
template <typename T>
concept parameter = std::is_default_constructible_v<decltype(T::value)>;

template <typename T>
concept int_parameter = requires(T t) {
                          {
                            t.value
                            } -> int_ish;
                        };

template <typename T>
concept float_parameter = requires(T t) {
                            {
                              t.value
                              } -> fp_ish;
                          };

template <typename T>
concept bool_parameter = requires(T t) {
                           {
                             t.value
                             } -> bool_ish;
                         };

template <typename T>
concept string_parameter = requires(T t) {
                             {
                               t.value
                               } -> std::convertible_to<std::string>;
                           };

template <typename C>
concept parameter_with_full_range = requires {
                                      avnd::get_range<C>().min;
                                      avnd::get_range<C>().max;
                                      avnd::get_range<C>().init;
                                    };

template <typename T>
concept enum_parameter = std::is_enum_v<decltype(T::value)>;

/**
 * A "control" is a parameter + some metadata:
 *
 * struct
 * {
 *   // see widgets.hpp
 *   enum widget { slider };
 *
 *
 *   static consteval auto range() {
 *     struct {
 *       float min = 0.;
 *       float max = 1.;
 *       float init = 0.25;
 *     } r;
 *     return r;
 *   }
 *
 *   float value;
 * };
 */

template <typename T>
concept control = parameter<T> && (has_range<T> || has_widget<T>);

/**
 * A value port is a parameter which is not a control.
 */
template <typename T>
concept value_port = parameter<T> && !
control<T>;

/**
 * A sample-accurate parameter has an additional "values" member
 * which allows to know at which sample did a control change
 */
template <typename T>
concept sample_accurate_parameter
    = parameter<T> && requires(T t) { t.value = t.values[1]; };

/**
 * Like control but sample-accurate
 */
template <typename T>
concept sample_accurate_control = sample_accurate_parameter<T> && control<T>;

/**
 * Like value_port but sample-accurate
 */
template <typename T>
concept sample_accurate_value_port = sample_accurate_parameter<T> && !
control<T>;

}
