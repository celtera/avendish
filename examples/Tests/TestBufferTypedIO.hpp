#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/buffer.hpp>
#include <halp/meta.hpp>

#include <cstdint>

namespace examples::tests
{
struct TestBufferTypedIO
{
  halp_meta(name, "Test buffer (typed float) I/O")
  halp_meta(c_name, "avnd_test_buffer_typed")
  halp_meta(category, "Tests/Buffer")
  halp_meta(uuid, "fbbed029-5fd8-450c-a083-8a4e63ef20d7")
  struct { halp::typed_cpu_buffer_input<"In", float> in; } inputs;
  struct { halp::typed_cpu_buffer_output<"Out", float> out; } outputs;
  void operator()()
  {
    auto& b = inputs.in.buffer;
    if(!b.elements || b.element_count <= 0)
      return;
    auto dst = outputs.out.create(b.element_count);
    for(int64_t i = 0; i < b.element_count; i++)
      dst[i] = b.elements[i];
    outputs.out.upload();
  }
};
}
