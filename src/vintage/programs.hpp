#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/audio_processor.hpp>
#include <vintage/helpers.hpp>
#include <vintage/vintage.hpp>

namespace vintage
{

struct programs_setup
{
  template <typename Self_T>
  void init(Self_T& effect)
  {
    using T = typename Self_T::effect_type;
    if constexpr (has_programs<T>)
    {
      effect.Effect::numPrograms = std::size(T::programs);
    }
  }
};

}
