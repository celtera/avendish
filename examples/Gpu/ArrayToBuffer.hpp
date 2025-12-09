#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace uo
{
struct ArrayToBuffer
{
public:
  halp_meta(name, "Array to buffer")
  halp_meta(c_name, "avnd_arraytobuffer")
  halp_meta(category, "Visuals")
  halp_meta(description, "CPU buffer -> GPU buffer")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/array_to_buffer.html")
  halp_meta(author, "ossia score")
  halp_meta(uuid, "3127c11b-61d3-4d5e-a896-8b56e628dfbc")

  struct
  {
    halp::val_port<"Input", std::vector<float>> main;
  } inputs;

  struct
  {
    halp::cpu_buffer_output<"Output"> main;
  } outputs;

  void operator()() noexcept
  {
    const int num_elements = inputs.main.value.size();
    outputs.main.buffer.raw_data = (unsigned char*)inputs.main.value.data();
    outputs.main.buffer.byte_size = inputs.main.value.size() * sizeof(float);
    outputs.main.buffer.byte_offset = 0;
    outputs.main.buffer.changed = true;
  }
};
}
