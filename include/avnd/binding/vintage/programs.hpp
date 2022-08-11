#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/vintage/helpers.hpp>
#include <avnd/binding/vintage/vintage.hpp>
#include <avnd/concepts/audio_processor.hpp>

namespace vintage
{

struct programs_setup
{
  template <typename Self_T>
  void init(Self_T& effect)
  {
    using T = typename Self_T::effect_type;
    if constexpr(has_programs<T>)
    {
      effect.Effect::numPrograms = std::size(T::programs);
    }
  }
};

}
