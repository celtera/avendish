#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>

namespace mo
{
struct MidiHiResOutput
{
  halp_meta(name, "Midi hi-res output")
  halp_meta(c_name, "MidiHiResOut")
  halp_meta(category, "Midi");
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Creates MIDI LSB/MSB from a 0-16384 or 0-1 value")
  halp_meta(uuid, "d6f5173b-b823-4571-b31f-660832b6132b")

  struct
  {
    halp::accurate<halp::val_port<"int", int>> integer;
    halp::accurate<halp::val_port<"float", float>> fp;
  } inputs;

  struct
  {
    halp::timed_callback<"msb", int> msb;
    halp::timed_callback<"lsb", int> lsb;
  } outputs;

  void operator()()
  {
    for(auto& [t, v] : inputs.integer.values)
    {
      const int32_t m = ossia::clamp(int32_t(v / 127), 0, 127);
      const int32_t l = ossia::clamp(int32_t(v - m * 127), 0, 127);

      outputs.msb(t, m);
      outputs.lsb(t, l);
    }
    for(auto& [t, val] : inputs.fp.values)
    {
      const double v = val * (128. * 128.);
      const int32_t m = ossia::clamp(int32_t(v / 127.), 0, 127);
      const int32_t l = ossia::clamp(int32_t(v - m * 127.), 0, 127);

      outputs.msb(t, m);
      outputs.lsb(t, l);
    }
  }
};
}
