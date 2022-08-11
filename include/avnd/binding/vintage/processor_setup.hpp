#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/vintage/helpers.hpp>

namespace vintage
{

struct ProcessorSetup
{
  template <typename Self_T>
  void init(Self_T& effect)
  {
    using T = typename Self_T::effect_type;

    // Mandatory
    effect.Effect::processReplacing
        = [](Effect* effect, float** inputs, float** outputs, int32_t sampleFrames) {
      auto& self = *static_cast<Self_T*>(effect);
      return self.process(inputs, outputs, sampleFrames);
    };

    if constexpr(avnd::double_processor<T>)
    {
      effect.Effect::processDoubleReplacing
          = [](Effect* effect, double** inputs, double** outputs, int32_t sampleFrames) {
        auto& self = *static_cast<Self_T*>(effect);
        return self.process(inputs, outputs, sampleFrames);
      };
    }
  }
};

}
