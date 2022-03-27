#pragma once
#include <random>

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace examples
{
struct Random
{
  static consteval auto name() { return "random"; }
  static consteval auto c_name() { return "avnd_random"; }
  static consteval auto uuid() { return "0c638609-20ca-48e7-a5d1-099f33dc944f"; }

  struct
  {
    struct
    {
      float value;
    } out;
  } outputs;

  void operator()()
  {
    static thread_local std::random_device dev;
    static thread_local std::mt19937_64 gen{dev()};

    auto dist = std::uniform_real_distribution<>{0., 1.};
    outputs.out.value = dist(gen);
  }
};
}
