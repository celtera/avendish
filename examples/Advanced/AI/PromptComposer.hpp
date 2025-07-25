#pragma once
#include <boost/algorithm/string.hpp>
#include <cmath>
#include <fmt/format.h>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/pod_vector.hpp>

#include <algorithm>
#include <iostream>

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

}
