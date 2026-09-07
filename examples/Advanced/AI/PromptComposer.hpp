#pragma once
#include <cmath>
#include <fmt/format.h>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <halp/string_list.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/small_vector.hpp>
#include <ossia/network/value/value.hpp>

#include <algorithm>
#include <vector>

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace ai
{
struct PromptComposer
{
  halp_meta(name, "Prompt composer")
  halp_meta(c_name, "prompt_composer")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "AI/Prompts")
  halp_meta(description, "Generate a prompt with percentages")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/prompt-composer.html")
  halp_meta(uuid, "a4227e94-cf7d-4776-9aa0-2f384be7d97f")

  struct inputs
  {
    struct keyword_list : halp::string_list<"Keywords">
    {
      // Retain the old empty keyword and its first dynamic weight by default.
      keyword_list() { value.emplace_back(12000, ""); }

      // Versioned by representation: old documents/presets stored a STRING;
      // current ones store LISTs of [stable port identity, keyword]. Preserve
      // all rows, including empty and trailing rows, and their old weight IDs.
      static ossia::value migrate_value(const ossia::value& previous)
      {
        if(auto text = previous.target<std::string>())
        {
          std::vector<ossia::value> rows;
          std::size_t start = 0;
          do
          {
            auto end = text->find('\n', start);
            rows.emplace_back(
                std::vector<ossia::value>{
                    12000 + int(rows.size()), text->substr(start, end - start)});
            if(end == std::string::npos)
              break;
            start = end + 1;
          } while(true);
          return rows;
        }
        return previous;
      }

      static std::function<void(PromptComposer&, const halp::string_list_value&)>
      on_controller_interaction()
      {
        return [](PromptComposer& object, const halp::string_list_value& rows) {
          object.inputs.in_i.set_rows(rows);
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

    halp::keyed_dynamic_port<weight_port> in_i;
  } inputs;

  struct
  {
    halp::val_port<"Output", std::string> out;
  } outputs;

  void operator()()
  {
    outputs.out.value.clear();
    const auto count
        = std::min(inputs.controller.value.size(), inputs.in_i.ports.size());
    for(std::size_t i = 0; i < count; ++i)
    {
      std::string_view text = inputs.controller.value[i].second;
      while(!text.empty() && static_cast<unsigned char>(text.front()) <= 32)
        text.remove_prefix(1);
      while(!text.empty() && static_cast<unsigned char>(text.back()) <= 32)
        text.remove_suffix(1);
      outputs.out.value += fmt::format("({}:{}), ", text, inputs.in_i.ports[i].value);
    }

    if(outputs.out.value.ends_with(", "))
    {
      outputs.out.value.pop_back();
      outputs.out.value.pop_back();
    }
  }
};


}
