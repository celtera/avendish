#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/process/base.hpp>

namespace avnd
{

template <typename T>
struct process_bus_adapter
{
  template <std::floating_point SrcFP>
  void allocate_buffers(process_setup setup, SrcFP f)
  {
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation, avnd::span<avnd::span<FP*>> in, avnd::span<avnd::span<FP*>> out,
      const auto& tick)
  {
    // Audio buffers aren't used at all
    invoke_effect(implementation, tick);
  }
};


}
