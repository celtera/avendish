#pragma once

#include <avnd/common/concepts_polyfill.hpp>
#include <ossia/dataflow/port.hpp>

namespace oscr
{
template <typename Port>
concept ossia_value_port = requires(Port p) {
  { p.value } -> std::convertible_to<ossia::value_port*>;
};
template <typename Port>
concept ossia_audio_port = requires(Port p) {
  { p.value } -> std::convertible_to<ossia::audio_port*>;
};
template <typename Port>
concept ossia_midi_port = requires(Port p) {
  { p.value } -> std::convertible_to<ossia::midi_port*>;
};
template <typename Port>
concept ossia_port
    = ossia_value_port<Port> || ossia_audio_port<Port> || ossia_midi_port<Port>;
}
