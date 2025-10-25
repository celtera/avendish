#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

namespace uo
{
struct BufferToArray
{
public:
  halp_meta(name, "Buffer to array")
  halp_meta(c_name, "avnd_buffertoarray")
  halp_meta(category, "Visuals")
  halp_meta(description, "GPU buffer -> CPU buffer")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/buffer_to_array.html")
  halp_meta(author, "ossia score")
  halp_meta(uuid, "5274a895-8232-4e79-bc7a-50c27274bd10")

  struct
  {
    halp::buffer_input<"Input"> main;
  } inputs;

  struct
  {
    halp::val_port<"Output", std::vector<float>> main;
  } outputs;

  void operator()() noexcept
  {
    auto sz = inputs.main.buffer.bytesize;
    if((sz % sizeof(float)) == 0) {
      outputs.main.value.resize(sz / sizeof(float));
      memcpy(outputs.main.value.data(), inputs.main.buffer.bytes, sz);
    }
  }
};
}
