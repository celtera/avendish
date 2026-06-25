#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.basic.hpp>
#include <halp/meta.hpp>

namespace examples::tests
{
struct TestPrepare
{
  halp_meta(name, "Test prepare")
  halp_meta(c_name, "avnd_test_prepare")
  halp_meta(category, "Tests/Lifecycle")
  halp_meta(uuid, "30a8c36d-56d7-45b6-aa2d-ca16520b3176")
  struct
  {
    halp::val_port<"Rate", float> rate;
    halp::val_port<"BufferSize", int> buffer_size;
  } outputs;
  double m_rate = 0.;
  int m_frames = 0;
  void prepare(halp::setup s)
  {
    m_rate = s.rate;
    m_frames = s.frames;
  }
  void operator()()
  {
    outputs.rate.value = static_cast<float>(m_rate);
    outputs.buffer_size.value = m_frames;
  }
};
}
