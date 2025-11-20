#pragma once
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

namespace ao
{
struct ValueMixer
{
  halp_meta(name, "Value Mixer")
  halp_meta(c_name, "avnd_mix_values")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Control/Mappings")
  halp_meta(
      description, "Average of N inputs: from 123, 4.56, 7.3 to (123 + 4.56 + 7.3) / 3")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#mix")
  halp_meta(uuid, "0fd108dd-daa8-4667-869d-f409da70e823")

  enum Mode
  {
    Mix,
    Weight,
    Min,
    Max,
    Multiply
  };
  struct ins
  {
    struct : halp::spinbox_i32<"Input count", halp::range{0, 1024, 2}>
    {
      static std::function<void(ValueMixer&, int)> on_controller_interaction()
      {
        return [](ValueMixer& object, int value) {
          object.inputs.in_i.request_port_resize(value);
          object.inputs.mix_i.request_port_resize(value);
          object.inputs.solo_i.request_port_resize(value);
          object.inputs.mute_i.request_port_resize(value);
        };
      }
    } controller;
    halp::enum_t<Mode, "Mode"> mix_mode;

    // FIXME for this usecase we could have instead a placeholder type ?
    halp::dynamic_port<halp::val_port<"Input {}", float>> in_i;
    halp::dynamic_port<halp::knob_f32<"Mix {}">> mix_i;
    halp::dynamic_port<halp::toggle<"Solo {}">> solo_i;
    halp::dynamic_port<halp::toggle<"Mute {}">> mute_i;
  } inputs;

  struct
  {
    halp::val_port<"Output", float> out;
  } outputs;

  template <typename Func>
  void foreach_active(int n, std::span<int> solo_indices, Func&& f)
  {
    if(solo_indices.empty())
    {
      for(int i = 0; i < n; i++)
      {
        if(inputs.mute_i.ports[i].value)
          continue;
        f(i);
      }
    }
    else
    {
      for(int i : solo_indices)
      {
        if(inputs.mute_i.ports[i].value)
          continue;
        f(i);
      }
    }
  }

  void combine_mix(int n, std::span<int> solo_indices)
  {
    double sum = 0.0;
    int active = 0;

    foreach_active(n, solo_indices, [&](int i) {
      const float input = inputs.in_i.ports[i].value;
      const float mix = inputs.mix_i.ports[i].value;
      sum += input * mix;
      active++;
    });

    if(active > 0)
      outputs.out.value = static_cast<float>(sum / active);
  }

  void combine_weighted(int n, std::span<int> solo_indices)
  {
    double weighted_sum = 0.0;
    double total_weight = 0.0;

    foreach_active(n, solo_indices, [&](int i) {
      const float input = inputs.in_i.ports[i].value;
      const float weight = inputs.mix_i.ports[i].value;
      weighted_sum += input * weight;
      total_weight += weight;
    });

    if(std::abs(total_weight) > 1e-7)
      outputs.out.value = static_cast<float>(weighted_sum / total_weight);
  }

  void combine_min(int n, std::span<int> solo_indices)
  {
    float current_min = std::numeric_limits<float>::max();
    bool any_active = false;

    foreach_active(n, solo_indices, [&](int i) {
      const float val = inputs.in_i.ports[i].value * inputs.mix_i.ports[i].value;
      if(val < current_min)
        current_min = val;
      any_active = true;
    });

    if(any_active)
      outputs.out.value = current_min;
  }

  void combine_max(int n, std::span<int> solo_indices)
  {
    float current_max = std::numeric_limits<float>::lowest();
    bool any_active = false;

    foreach_active(n, solo_indices, [&](int i) {
      const float val = inputs.in_i.ports[i].value * inputs.mix_i.ports[i].value;
      if(val > current_max)
        current_max = val;
      any_active = true;
    });

    if(any_active)
      outputs.out.value = current_max;
  }

  void combine_multiply(int n, std::span<int> solo_indices)
  {
    float product = 1.0f;
    bool any_active = false;

    foreach_active(n, solo_indices, [&](int i) {
      product *= (inputs.in_i.ports[i].value * inputs.mix_i.ports[i].value);
      any_active = true;
    });

    outputs.out.value = any_active ? product : 0.0f;
  }

  void operator()()
  {
    outputs.out.value = 0.;
    int n = inputs.in_i.ports.size();
    if(n == 0)
      return;

    boost::container::small_vector<int, 16> solo_indices;
    int i = 0;
    for(auto& port : inputs.solo_i.ports)
    {
      if(port.value)
        solo_indices.push_back(i);
      i++;
    }

    switch(inputs.mix_mode)
    {
      case Mode::Mix:
        return combine_mix(n, solo_indices);
      case Mode::Weight:
        return combine_weighted(n, solo_indices);
      case Mode::Min:
        return combine_min(n, solo_indices);
      case Mode::Max:
        return combine_max(n, solo_indices);
      case Mode::Multiply:
        return combine_multiply(n, solo_indices);
    }
  }
};
}
