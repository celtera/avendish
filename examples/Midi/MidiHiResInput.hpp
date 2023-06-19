#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>

namespace mo
{
struct MidiHiResInput
{
  halp_meta(name, "Midi hi-res input")
  halp_meta(c_name, "MidiHiResIn")
  halp_meta(category, "Midi");
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Creates a float from MSB/LSB CCs")
  halp_meta(uuid, "28ca746e-c304-4ba6-bd5b-78934a1dec55")

  boost::container::small_flat_map<int, std::pair<int, int>, 8> current;
  struct
  {
    halp::accurate<halp::val_port<"msb", int>> msb;
    halp::accurate<halp::val_port<"lsb", int>> lsb;
  } inputs;

  struct
  {
    halp::timed_callback<"int", int> integer;
    halp::timed_callback<"float", float> fp;
  } outputs;

  void operator()()
  {
    auto& msbs = inputs.msb.values;
    auto& lsbs = inputs.lsb.values;
    if(msbs.empty() && lsbs.empty())
      return;

    current.clear();

    // 1. Sort all the MSBs, LSBs in a single array
    for(auto& [time, val] : msbs)
    {
      current[time].first = val;
      current[time].second = -1;
    }
    for(auto& [time, val] : lsbs)
    {
      if(auto it = current.find(time); it != current.end())
      {
        it->second.second = val;
      }
      else
      {
        current[time].first = -1;
        current[time].second = val;
      }
    }

    int current_msb = inputs.msb.value;
    int current_lsb = inputs.lsb.value;

    // 2. Send them
    for(auto& [time, pair] : current)
    {
      auto& [m, l] = pair;
      if(m == -1)
        m = current_msb;
      else
        current_msb = m;

      if(l == -1)
        l = current_lsb;
      else
        current_lsb = l;

      assert(pair.first != -1);
      assert(pair.second != -1);

      const int res = m * 127 + l;
      outputs.integer(time, res);
      outputs.fp(time, double(res) / (128. * 128.));
    }
  }
};
}
