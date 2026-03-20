#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/concepts/callback.hpp>
#include <avnd/concepts/parameter.hpp>

namespace avnd
{

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
concept control_port = /*(parameter<T> || callback<T>) &&*/ parameter_port<T>
                       && (/*has_range<T> || */ has_widget<T>);

// FIXME all this needs improving: they do not support callbacks.
// Also messages are not handled.
// Ideally, messages, callbacks and controls would all be handled exactly in the same way ?
template <typename T>
concept int_control_port = int_parameter<T> && control_port<T>;
template <typename T>
concept enum_control_port = enum_parameter<T> && control_port<T>;
template <typename T>
concept float_control_port = float_parameter<T> && control_port<T>;
template <typename T>
concept bool_control_port = bool_parameter<T> && control_port<T>;
template <typename T>
concept string_control_port = string_parameter<T> && control_port<T>;
template <typename T>
concept time_control_port = float_control_port<T> && requires { T::time_chooser; };

template <typename T>
concept int_value_port = int_parameter<T> && !control_port<T>;
template <typename T>
concept enum_value_port = enum_parameter<T> && !control_port<T>;
template <typename T>
concept float_value_port = float_parameter<T> && !control_port<T>;
template <typename T>
concept bool_value_port = bool_parameter<T> && !control_port<T>;
template <typename T>
concept string_value_port = string_parameter<T> && !control_port<T>;

/**
 * A value port is a parameter which is not a control.
 */
template <typename T>
concept value_port = parameter_port<T> && !control_port<T>;

template <typename T>
concept span_control_port = span_parameter_port<T> && control_port<T>;
template <typename T>
concept span_value_port = span_parameter_port<T> && value_port<T>;

/**
 * Like control but sample-accurate
 */
template <typename T>
concept sample_accurate_control_port
    = sample_accurate_parameter_port<T> && control_port<T>;

/**
 * Like value_port but sample-accurate
 */
template <typename T>
concept sample_accurate_value_port
    = sample_accurate_parameter_port<T> && !control_port<T>;
}
