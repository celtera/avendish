#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <vector>
#include <optional>
#include <iostream>
#include <map>
#include <span>
#include <avnd/concepts/parameter.hpp>
namespace examples
{
/**
 * Low-pass, but with timestamped controls
 */
struct SampleAccurateControls
{
  static consteval auto name() { return "Sample accurate example"; }
  static consteval auto c_name() { return "avnd_sampleaccurate"; }
  static consteval auto uuid()
  {
    return "e6c34e9e-fc66-44d4-8798-dd3cd67b7fb2";
  }

  struct inputs_t
  {
    struct
    {
      static consteval auto name() { return "API A"; }

      struct range
      {
        const float min = 0.;
        const float max = 1.;
        const float init = 0.5;
      };

      std::optional<float>* values;
      float value{};
    } weight_A;

    struct
    {
      static consteval auto name() { return "API B"; }

      struct range
      {
          const float min = 0.;
          const float max = 1.;
          const float init = 0.5;
      };

      struct timestamped_value {
        float value;
        int frame;
      };
      std::span<timestamped_value> values{};
      float value{};
    } weight_B;

    struct
    {
      static consteval auto name() { return "API C"; }

      struct range
      {
        const float min = 0.;
        const float max = 1.;
        const float init = 0.5;
      };

      std::map<int, float> values{};
      float value{};
    } weight_C;
  } inputs;


  inputs_t outputs;

  void operator()(int N)
  {
    // For the "values" members, each control change is associated with a frame index.
    for(int i = 0; i < N; i++)
    {
      // In this case the values are allocated with as many elements as there are audio frames
      // in the audio buffer.
      // Ideally there should be a way to optimize the storage so that all the values are first,
      // and all the bools packed at the end... FIXME !
      if(inputs.weight_A.values[i]) {
        std::cout << "Output on A[" << i << "]: " << *inputs.weight_A.values[i] << std::endl;
      }
    }

    // In this case
    for(auto& val : inputs.weight_B.values) {
      std::cout << "Output on B[" << val.frame << "]: " << val.value << std::endl;
    }

    // the "value" member represents the last known value
  }
};
}
