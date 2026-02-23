#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{
template <typename T>
concept dynamic_ports_port = requires(T t) {
  t.ports[0];
  t.request_port_resize;
};

template <typename T>
using dynamic_port_type =
    typename std::decay_t<decltype(std::declval<T>().ports)>::value_type;

template <typename T>
struct concrete_port_type_getter
{
  using type = T;
};

template <avnd::dynamic_ports_port T>
struct concrete_port_type_getter<T>
{
  using type = avnd::dynamic_port_type<T>;
};

template <typename T>
using concrete_port_type = typename concrete_port_type_getter<T>::type;
}
