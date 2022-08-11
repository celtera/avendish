#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/function_reflection.hpp>
#include <avnd/concepts/generic.hpp>

#include <string_view>

namespace avnd
{

type_or_value_qualification(ui)
type_or_value_reflection(ui)

type_or_value_qualification(layout)
type_or_value_reflection(layout)
template <typename T>
concept hbox_layout = (T::layout() == decltype(T::layout())::hbox)
                      || (T::layout == decltype(T::layout)::hbox);
template <typename T>
concept vbox_layout = (T::layout() == decltype(T::layout())::vbox)
                      || (T::layout == decltype(T::layout)::vbox);
template <typename T>
concept grid_layout = (T::layout() == decltype(T::layout())::grid)
                      || (T::layout == decltype(T::layout)::grid);
template <typename T>
concept split_layout = (T::layout() == decltype(T::layout())::split)
                       || (T::layout == decltype(T::layout)::split);
template <typename T>
concept group_layout = (T::layout() == decltype(T::layout())::group)
                       || (T::layout == decltype(T::layout)::group);
template <typename T>
concept container_layout = (T::layout() == decltype(T::layout())::container)
                           || (T::layout == decltype(T::layout)::container);
template <typename T>
concept tab_layout = (T::layout() == decltype(T::layout())::tabs)
                     || (T::layout == decltype(T::layout)::tabs);
template <typename T>
concept spacing_layout = (T::layout() == decltype(T::layout())::spacing)
                         || (T::layout == decltype(T::layout)::spacing);
template <typename T>
concept control_layout = (T::layout() == decltype(T::layout())::control)
                         || (T::layout == decltype(T::layout)::control);
template <typename T>
concept custom_layout = (T::layout() == decltype(T::layout())::custom)
                        || (T::layout == decltype(T::layout)::custom);

}
