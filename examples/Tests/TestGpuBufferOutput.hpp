#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestGpuBufferOutput
{
  halp_meta(name, "Test GPU buffer output")
  halp_meta(c_name, "avnd_test_gpu_buffer_out")
  halp_meta(category, "Tests/Buffer")
  halp_meta(uuid, "95420a8f-c1f9-40df-b108-7c3cf6adc7a7")
  struct { halp::knob_i32<"Bytes", halp::range{1, 1048576, 1024}> bytes; } inputs;
  struct { halp::gpu_buffer_output<"Out"> out; } outputs;
  void operator()()
  {
    const int64_t bytes = inputs.bytes;
    auto* data = outputs.out.allocate(bytes);
    for(int64_t i = 0; i < bytes; i++)
      data[i] = static_cast<unsigned char>(i & 0xFF);
  }
};
}
