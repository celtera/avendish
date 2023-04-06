#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

namespace avnd
{

template <typename T>
concept port_can_process = requires(T t) { T::process({}); };

template <typename T>
concept has_worker = requires(T t) { t.worker.request = {}; };

}
