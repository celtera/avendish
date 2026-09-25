#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace ao
{
enum class ArrayBestMode
{
  Highest, // similarities, scores
  Lowest,  // distances
};

/**
 * The best element of an array: its index and value, and a softmax of the
 * whole array. Pairs with the Array Value Combiner's similarity and distance
 * modes: "which label matches the image", "which reference pose is closest".
 */
struct ArrayBest
{
  halp_meta(name, "Array Best Match")
  halp_meta(c_name, "avnd_array_best")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Control/Mappings")
  halp_meta(
      description,
      "Index and value of the highest (or lowest) element of an array, and the "
      "softmax of the array as probabilities.")
  halp_meta(uuid, "9e793245-72a1-4aa2-a813-5f5836cf1637")

  struct
  {
    halp::val_port<"Input", std::vector<float>> in;
    struct : halp::enum_t<ao::ArrayBestMode, "Mode">
    {
      halp_meta(description, "Highest for similarities and scores, Lowest for distances");
    } mode;
    struct : halp::knob_f32<"Softmax scale", halp::range{0., 1000., 1.}>
    {
      halp_meta(
          description,
          "Sharpness of the probabilities. CLIP multiplies its cosine "
          "similarities by 100.")
    } scale;
  } inputs;

  struct
  {
    halp::val_port<"Index", int> index;
    halp::val_port<"Value", float> value;
    halp::val_port<"Probabilities", std::vector<float>> probabilities;
  } outputs;

  void operator()()
  {
    const auto& in = inputs.in.value;
    auto& p = outputs.probabilities.value;
    if(in.empty())
    {
      outputs.index.value = -1;
      outputs.value.value = 0.f;
      p.clear();
      return;
    }

    const bool lowest = inputs.mode.value == ArrayBestMode::Lowest;
    const auto it = lowest ? std::min_element(in.begin(), in.end())
                           : std::max_element(in.begin(), in.end());
    outputs.index.value = int(it - in.begin());
    outputs.value.value = *it;

    // exp(s * (x - best)) for Highest, exp(-s * (x - best)) for Lowest: the
    // best element is exp(0) = 1, so nothing overflows.
    const double s = lowest ? -inputs.scale.value : inputs.scale.value;
    const double best = *it;
    p.resize(in.size());
    double sum = 0.;
    for(std::size_t i = 0; i < in.size(); i++)
    {
      const double e = std::exp(s * (in[i] - best));
      p[i] = float(e);
      sum += e;
    }
    for(auto& x : p)
      x = float(x / sum);
  }
};
}
