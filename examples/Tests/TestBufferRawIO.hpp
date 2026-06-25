#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/buffer.hpp>
#include <halp/meta.hpp>

#include <cstring>

namespace examples::tests
{
struct TestBufferRawIO
{
  halp_meta(name, "Test buffer (raw) I/O")
  halp_meta(c_name, "avnd_test_buffer_raw")
  halp_meta(category, "Tests/Buffer")
  halp_meta(uuid, "c4a9bf63-ecf3-4c55-93a8-ce4d11fd79da")
  struct { halp::cpu_buffer_input<"In"> in; } inputs;
  struct { halp::cpu_buffer_output<"Out"> out; } outputs;
  void operator()()
  {
    auto& b = inputs.in.buffer;
    if(!b.raw_data || b.byte_size <= 0)
      return;
    auto dst = outputs.out.create<unsigned char>(b.byte_size);
    std::memcpy(dst.data(), b.raw_data, b.byte_size);
    outputs.out.upload();
  }
};
}
