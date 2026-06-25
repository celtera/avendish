#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

#include <functional>

namespace examples::tests
{
struct TestWorker
{
  halp_meta(name, "Test worker")
  halp_meta(c_name, "avnd_test_worker")
  halp_meta(category, "Tests/Threading")
  halp_meta(uuid, "8458884e-d600-44bf-8e86-31d718ad06ec")
  struct { halp::val_port<"In", int> in; } inputs;
  struct { } outputs;
  void operator()() { worker.request(inputs.in.value); }
  struct worker
  {
    std::function<void(int)> request;
    static void work(int) { }
  } worker;
};
}
