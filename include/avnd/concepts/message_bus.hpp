#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{
template<typename T>
concept message_bus = requires (T t, typename T::ui ui, typename T::ui::bus bus)
{
  t.send_message({});
  t.process_message({});

  bus.init(ui);
  bus.process_message(ui, {});
  bus.send_message({});
};

}
