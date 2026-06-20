// SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD

// The `halp` C++20 module: exports the halp convenience layer so user objects can
// `import halp;` instead of including the (heavy) halp headers. Only halp's own
// dependencies are pulled in -- no binding / score / ossia / Qt -- so this builds
// standalone. Built into the `avnd_halp_module` target (CMAKE_CXX_SCAN_FOR_MODULES).
//
// The global module fragment includes everything halp transitively needs so those
// declarations stay attached to the global module; the purview then includes the
// halp headers with HALP_MODULE_BUILD set, which turns their HALP_MODULE_EXPORT
// markers into real `export`s.

module;

// --- std ---
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <complex>
#include <concepts>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <initializer_list>
#include <iosfwd>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <numeric>
#include <optional>
#include <ostream>
#include <ratio>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <version>

// --- halp's third-party + avnd dependencies (top-level roots; each pulls its tree) ---
#include <avnd/common/enum_reflection.hpp>
#include <avnd/common/message_tag.hpp>
#include <avnd/common/metadata_tag.hpp>
#include <avnd/common/span_polyfill.hpp>
#include <avnd/introspection/input.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/predef.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>

export module halp;

#define HALP_MODULE_BUILD 1

// --- the exported halp API ---
// Note: halp/gradient_port.hpp is intentionally omitted -- it instantiates a
// boost::container::flat_map in an exported context, which exposes a TU-local
// entity (ill-formed in a module interface). Gradient ports remain header-only.
#include <halp/attributes.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/controls_fmt.hpp>
#include <halp/curve.hpp>
#include <halp/custom_widgets.hpp>
#include <halp/device.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/fft.hpp>
#include <halp/file_port.hpp>
#include <halp/geometry.hpp>
#include <halp/inline.hpp>
#include <halp/layout.hpp>
#include <halp/log.hpp>
#include <halp/mappers.hpp>
#include <halp/messages.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/midifile_port.hpp>
#include <halp/modules.hpp>
#include <halp/polyfill.hpp>
#include <halp/reactive_value.hpp>
#include <halp/sample_accurate_controls.hpp>
#include <halp/schedule.hpp>
#include <halp/shared_instance.hpp>
#include <halp/smooth_controls.hpp>
#include <halp/smoothers.hpp>
#include <halp/soundfile_port.hpp>
#include <halp/static_string.hpp>
#include <halp/texture.hpp>
#include <halp/texture_formats.hpp>
#include <halp/value_types.hpp>

#include <halp/controls.basic.hpp>
#include <halp/controls.buttons.hpp>
#include <halp/controls.enums.hpp>
#include <halp/controls.sliders.hpp>
#include <halp/controls.typedefs.hpp>
