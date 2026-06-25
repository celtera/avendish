#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.basic.hpp>
#include <halp/messages.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestMessages
{
  halp_meta(name, "Test messages")
  halp_meta(c_name, "avnd_test_messages")
  halp_meta(category, "Tests/Messages")
  halp_meta(uuid, "9d97a619-9252-407d-aa78-3f0fe75b63d9")
  struct { halp::val_port<"Count", int> count; } outputs;

  void ping() { outputs.count.value++; }
  void add(int x) { outputs.count.value += x; }

  halp_start_messages(TestMessages)
    halp_mem_fun(ping)
    halp_mem_fun(add)
  halp_end_messages
};
}
