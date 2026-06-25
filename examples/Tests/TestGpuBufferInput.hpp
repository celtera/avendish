#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/buffer.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestGpuBufferInput
{
  halp_meta(name, "Test GPU buffer input")
  halp_meta(c_name, "avnd_test_gpu_buffer_in")
  halp_meta(category, "Tests/Buffer")
  halp_meta(uuid, "f086a4b9-8301-4677-bc0f-be1b707459c0")
  struct { halp::gpu_buffer_input<"In"> in; } inputs;
  struct { halp::val_port<"HasData", bool> has; } outputs;
  void operator()() { outputs.has.value = inputs.in.buffer.handle != nullptr; }
};
}
