#pragma once
#include <boost/algorithm/string.hpp>
#include <cmath>
#include <fmt/format.h>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>

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

    halp::dynamic_port<halp::knob_f32<"Input {}">> in_i;
  } inputs;

  struct
  {
    halp::val_port<"Output", std::string> out;
  } outputs;

  void operator()()
  {
    outputs.out.value = "";
    thread_local std::vector<std::string> strs;
    static const auto loc = std::locale("C");
    strs.clear();
    boost::split(strs, inputs.controller.value, boost::is_any_of("\n"));
    auto it = strs.begin();
    for(auto& val : inputs.in_i.ports)
    {
      boost::trim(*it, loc);
      outputs.out.value += fmt::format("({}:{}), ", *it, val.value);
      ++it;
    }
    if(outputs.out.value.ends_with(", "))
    {
      outputs.out.value.pop_back();
      outputs.out.value.pop_back();
    }
  }
};

}
