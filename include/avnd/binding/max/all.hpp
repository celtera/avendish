#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/audio_processor.hpp>
#include <avnd/binding/max/message_processor.hpp>
#include <avnd/binding/max/jitter_processor_v2.hpp>
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>

namespace max
{

/**
 * Factory selector for Max objects
 * Automatically chooses the right processor type based on the ports
 */
template<typename T>
struct processor_selector
{
  // Check for texture ports (Jitter)
  static constexpr bool has_texture_ports =
    avnd::cpu_texture_input_introspection<T>::size > 0 ||
    avnd::cpu_texture_output_introspection<T>::size > 0;

  // Check for audio ports (MSP)
  static constexpr bool has_audio_ports =
    avnd::audio_input_introspection<T>::size > 0 ||
    avnd::audio_output_introspection<T>::size > 0;

  // Select the appropriate processor type
  using type = std::conditional_t<
    has_texture_ports,
    jitter_processor_metaclass<T>,     // Use Jitter for texture processing
    std::conditional_t<
      has_audio_ports,
      audio_processor_metaclass<T>,      // Use MSP for audio processing
      message_processor_metaclass<T>     // Use regular Max for everything else
    >
  >;
};

/**
 * Main factory function
 * This is what should be called from the binding entry point
 */
template<typename T>
inline auto make_max_processor()
{
  using processor = typename processor_selector<T>::type;
  static processor instance;
  return &instance;
}

} // namespace max