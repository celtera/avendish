#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

namespace avnd
{

template <typename T>
concept has_processor_to_gui_bus = requires(T t) {
                                     // The processor can send a message to the GUI
                                     t.send_message({});

                                     // The GUI can process a message
                                     std::declval<typename T::ui::bus>().process_message(
                                         std::declval<typename T::ui&>(), {});
                                   };

template <typename T>
concept has_gui_to_processor_bus
    = requires(T t) {
        // The processor can receive a message from the GUI
        t.process_message({});

        // The GUI can send a message to the processor
        std::declval<typename T::ui::bus>().send_message({});
      };
}
