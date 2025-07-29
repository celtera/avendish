#pragma once
#include <boost/algorithm/string.hpp>
#include <cmath>
#include <fmt/format.h>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/small_vector.hpp>

#include <algorithm>

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace ai
{
struct PromptComposer
{
  halp_meta(name, "Prompt composer")
  halp_meta(c_name, "prompt_composer")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "AI")
  halp_meta(description, "Generate a prompt with percentages")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/prompt-composer.html")
  halp_meta(uuid, "a4227e94-cf7d-4776-9aa0-2f384be7d97f")

  struct inputs
  {
    struct : halp::lineedit<"Keywords", "">
    {
      static std::function<void(PromptComposer&, const std::string&)>
      on_controller_interaction()
      {
        return [](PromptComposer& object, std::string_view value) {
          int n = std::count(value.begin(), value.end(), '\n');
          object.inputs.in_i.request_port_resize(n + 1);
        };
      }
    } controller;

    struct : halp::val_port<"Weights", ossia::small_pod_vector<float, 8>>
    {
      void update(PromptComposer& obj)
      {
        int N = std::min(value.size(), obj.inputs.in_i.ports.size());
        auto& w = obj.inputs.in_i.ports;
        for(int i = 0; i < N; i++)
        {
          w[i].value = value[i];
        }
      }
    } weights;
    struct weight_port : halp::knob_f32<"Input {}">
    {
      void update(PromptComposer& obj) { }
    };

    halp::dynamic_port<weight_port> in_i;
  } inputs;

  struct
  {
    halp::val_port<"Output", std::string> out;
  } outputs;

  void operator()()
  {
    outputs.out.value = "";
    splitted.clear();
    boost::split(splitted, inputs.controller.value, boost::is_any_of("\n"));
    auto it = splitted.begin();
    for(auto& val : inputs.in_i.ports)
    {
      boost::trim_if(*it, [](char c) { return c <= 32; });
      outputs.out.value += fmt::format("({}:{}), ", *it, val.value);
      ++it;
    }

    if(outputs.out.value.ends_with(", "))
    {
      outputs.out.value.pop_back();
      outputs.out.value.pop_back();
    }
  }

  std::vector<std::string> splitted;
};

struct PromptInterpolator
{
  halp_meta(name, "Prompt tweener")
  halp_meta(c_name, "prompt_interpolator")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "AI")
  halp_meta(description, "Interpolate between prompts over time")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/prompt-composer.html")
  halp_meta(uuid, "9238f0ec-df6a-446a-be5c-ec93e7355468")

  struct inputs
  {
    struct : halp::lineedit<"Prompt", "">
    {
      void update(PromptInterpolator& self) { self.on_new_prompt(); }
    } prompt;
    halp::time_chooser<"Duration"> duration;
  } inputs;

  struct outputs
  {
    halp::val_port<"Output", std::string> out;
  } outputs;

  struct weights
  {
    std::string prompt;
    float weight;
    float target_weight;
  };

  using tick = halp::tick_flicks;
  void operator()(const halp::tick_flicks& frames) noexcept
  {
    current_time = frames.start_in_flicks;
    update_interpolation(frames);
    generate_output();
  }

private:
  void on_new_prompt()
  {
    auto& new_prompt = inputs.prompt.value;
    if(new_prompt == last_prompt || new_prompt.empty())
      return;
    last_prompt = new_prompt;

    auto it = std::find_if(
        prompt_weights.begin(), prompt_weights.end(),
        [&](const weights& pw) { return pw.prompt == new_prompt; });

    if(it == prompt_weights.end())
    {
      for(auto& pw : prompt_weights)
        pw.target_weight = 0.0f;

      prompt_weights.push_back({new_prompt, 0.0f, 1.0f});
    }
    else
    {
      for(auto& pw : prompt_weights)
        pw.target_weight = (pw.prompt == new_prompt) ? 1.0f : 0.0f;
    }
    
    interpolation_start_time = current_time;
  }

  void update_interpolation(const halp::tick_flicks& frames)
  {
    if (prompt_weights.empty()) return;

    auto elapsed = (current_time - interpolation_start_time) / 705'600'000.0;
    double progress = std::min(1.0, elapsed / inputs.duration.value);

    for(auto& pw : prompt_weights)
    {
      pw.weight = pw.weight + (pw.target_weight - pw.weight) * progress;
    }

    prompt_weights.erase(
        std::remove_if(
            prompt_weights.begin(), prompt_weights.end(),
            [](const weights& pw) {
      return pw.weight < 0.001f && pw.target_weight < 0.001f;
    }),
        prompt_weights.end());
  }

  void generate_output()
  {
    outputs.out.value.clear();
    
    if (prompt_weights.empty()) return;
    
    bool first = true;
    for (const auto& pw : prompt_weights)
    {
      if (pw.weight > 0.001f)
      {
        if (!first) outputs.out.value += ", ";
        outputs.out.value += fmt::format("({}:{:.3f})", pw.prompt, pw.weight);
        first = false;
      }
    }
  }

  std::vector<weights> prompt_weights;
  std::string last_prompt;
  int64_t current_time{};
  int64_t interpolation_start_time{};
};

}
