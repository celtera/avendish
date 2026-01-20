#pragma once

#include <ossia/network/value/value.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>

#include <cmath>
#include <algorithm>
#include <limits>
#include <variant>
#include <vector>

namespace ao
{

class MultiChoice
{
public:
  halp_meta(name, "Multi-choice")
  halp_meta(c_name, "multi_choice")
  halp_meta(category, "Control/Mappings")
  halp_meta(uuid, "2c1d4578-7ef7-48b1-bbb8-c2b1c41063c9")
  halp_meta(author, "ossia score")
  halp_meta(description, "Choose a value according to multiple inputs")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/multi-choice.html")

  struct ins
  {
    struct : halp::val_port<"Inputs", std::vector<float>>
    {
    } nodes;

    halp::knob_f32<"Smooth", halp::range{.min = 0.01, .max = 1.0, .init = 0.1}> smooth;
    halp::knob_f32<"Threshold", halp::range{.min = 0.0, .max = 1.0, .init = 0.8}> threshold;
    halp::knob_f32<"Margin", halp::range{.min = 0.0, .max = 1.0, .init = 0.15}> margin;
  } inputs;

  struct outs
  {
    halp::val_port<"Output index", std::optional<int>> index;
    halp::val_port<"Current Weights", std::vector<float>> weights;
  } outputs;

  std::vector<float> m_state;
  std::optional<int> m_last_index;

  using tick = halp::tick;
  void operator()(halp::tick t)
  {
    const float alpha = this->inputs.smooth.value;
    const float threshold = 0.8f;
    const float margin = 0.15f;

    const std::vector<float>& input = this->inputs.nodes.value;
    if(input.empty())
    {
      m_last_index = std::nullopt;
      outputs.index = std::nullopt;
      outputs.weights.value.clear();
      return;
    }

    if(m_state.size() != input.size())
    {
      m_state.resize(input.size(), 0.0f);
      outputs.weights.value.resize(input.size(), 0.0f);
    }

    for(size_t i = 0; i < input.size(); ++i)
    {
      m_state[i] += (input[i] - m_state[i]) * alpha;
      outputs.weights.value[i] = m_state[i];
    }

    auto max_it = std::max_element(m_state.begin(), m_state.end());
    int winner_idx = std::distance(m_state.begin(), max_it);
    float winner_val = *max_it;

    float runner_up_val = 0.0f;
    for(size_t i = 0; i < m_state.size(); ++i)
    {
      if((int)i != winner_idx)
      {
        if(m_state[i] > runner_up_val)
          runner_up_val = m_state[i];
      }
    }

    if(winner_val > threshold && (winner_val - runner_up_val) > margin)
    {
      if(m_last_index != winner_idx)
        outputs.index = winner_idx;
      m_last_index = winner_idx;
    }
    else
    {
      outputs.index = std::nullopt;
      m_last_index = std::nullopt;
    }
  }
};

}
