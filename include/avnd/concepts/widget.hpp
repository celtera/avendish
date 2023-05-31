#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <type_traits>

namespace avnd
{

/// Widget reflection ///
template <typename C>
concept has_widget = requires { std::is_enum_v<typename C::widget>; };

}
