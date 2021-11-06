#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/helpers/reactive_value.hpp>

namespace examples
{

struct Addition
{
  static consteval auto name() { return "Addition"; }
  static consteval auto c_name() { return "addition"; }
  static consteval auto uuid()
  {
    return "21bf52fe-487b-426b-8fc8-ea9d7249fd3c";
  }

  struct
  {
    struct
    {
      float value;
    } a;
  } inputs;

  struct
  {
    struct
    {
      avnd::reactive_value<float> value;
    } out;
  } outputs;

  void operator()() { outputs.out.value = inputs.a.value * 100; }
};
}
