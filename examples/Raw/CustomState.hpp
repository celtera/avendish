#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cstdint>
#include <cstring>
#include <vector>

namespace examples
{
/**
 * Example of the custom host-state (session persistence) feature:
 * see STATE_SUPPORT_DESIGN.md and <avnd/concepts/state.hpp>.
 *
 * A processor may carry state beyond its parameter ports -- pattern data,
 * captured buffers, generated content... By exposing save_state / load_state,
 * that state is persisted in the DAW session by the plug-in bindings
 * (currently CLAP and VST3), appended to the usual parameter persistence.
 *
 * The blob is entirely owned by the processor: version it, validate it,
 * and reject anything suspicious by returning false (the bindings then
 * keep the current state and report failure to the host).
 */
struct CustomState
{
  static constexpr auto name() { return "CustomState"; }
  static constexpr auto c_name() { return "avnd_custom_state"; }
  static constexpr auto uuid() { return "3183d03e-2f43-4c4b-9a1e-6f5f2a3d9f4b"; }

  struct
  {
    struct
    {
      static constexpr auto name() { return "Level"; }
      struct range
      {
        double min = 0., max = 1., init = 0.5;
      };
      double value = 0.5;
    } level;
  } inputs;

  struct
  {
  } outputs;

  // Some state that is not expressible as parameters: here, a little
  // event pattern the user could edit through a custom UI.
  std::vector<uint8_t> pattern = {1, 0, 0, 1};

  static constexpr uint32_t state_magic = 0x50415431; // 'PAT1'

  std::vector<uint8_t> save_state()
  {
    std::vector<uint8_t> blob(8 + pattern.size());
    const uint32_t size = uint32_t(pattern.size());
    std::memcpy(blob.data(), &state_magic, 4);
    std::memcpy(blob.data() + 4, &size, 4);
    std::memcpy(blob.data() + 8, pattern.data(), pattern.size());
    return blob;
  }

  bool load_state(const uint8_t* data, size_t n)
  {
    if(n < 8)
      return false;
    uint32_t magic{}, size{};
    std::memcpy(&magic, data, 4);
    std::memcpy(&size, data + 4, 4);
    if(magic != state_magic || n != 8 + size_t(size) || size > 4096)
      return false;
    pattern.assign(data + 8, data + 8 + size);
    return true;
  }

  double operator()(double in)
  {
    // (The pattern would do something interesting here.)
    return in * inputs.level.value;
  }
};
}
