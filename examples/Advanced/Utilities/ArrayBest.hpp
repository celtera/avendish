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
      "softmax of the array as probabilities. Non-finite elements are skipped.")
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
    p.assign(in.size(), 0.f);

    // Only finite elements compete: NaN would make min / max meaningless and
    // an infinity would turn x - best into NaN.
    const bool lowest = inputs.mode.value == ArrayBestMode::Lowest;
    int best_i = -1;
    for(std::size_t i = 0; i < in.size(); i++)
      if(std::isfinite(in[i])
         && (best_i < 0 || (lowest ? in[i] < in[best_i] : in[i] > in[best_i])))
        best_i = int(i);

    outputs.index.value = best_i;
    if(best_i < 0)
    {
      outputs.value.value = 0.f;
      return; // empty, or nothing finite: all probabilities 0
    }
    const double best = in[best_i];
    outputs.value.value = in[best_i];

    // A message can bypass the control's range: keep the scale in [0, 1000],
    // NaN included (it fails both comparisons).
    double scale = inputs.scale.value;
    if(!(scale >= 0.))
      scale = 0.;
    else if(scale > 1000.)
      scale = 1000.;

    // Every exponent is <= 0 (x - best <= 0 for Highest, >= 0 for Lowest) and
    // finite, and the best element's is 0: each term is in [0, 1] and the sum
    // is at least 1.
    const double s = lowest ? -scale : scale;
    double sum = 0.;
    for(std::size_t i = 0; i < in.size(); i++)
    {
      if(!std::isfinite(in[i]))
        continue;
      const double e = std::exp(s * (in[i] - best));
      p[i] = float(e);
      sum += e;
    }
    for(auto& x : p)
      x = float(x / sum);
  }
};
}
