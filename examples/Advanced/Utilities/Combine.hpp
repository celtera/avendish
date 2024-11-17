#pragma once
#include <cmath>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

namespace ao
{
struct Combine
{
  halp_meta(name, "Combine")
  halp_meta(c_name, "avnd_combine")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Control/Mappings")
  halp_meta(description, "Combine N inputs in a list")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#combine")
  halp_meta(uuid, "3bbf74cd-55c1-473f-a11c-25ffec5b5c71")

  struct
  {
    struct : halp::spinbox_i32<"Control", halp::range{0, 1024, 1}>
    {
      static std::function<void(Combine&, int)> on_controller_interaction()
      {
        return [](Combine& object, int value) {
          object.inputs.in_i.request_port_resize(value);
        };
      }
    } controller;

    // FIXME for this usecase we could have instead a placeholder type ?
    halp::dynamic_port<halp::val_port<"Input {}", ossia::value>> in_i;
  } inputs;

  struct
  {
    halp::val_port<"Output", std::vector<ossia::value>> out;
  } outputs;

  void operator()()
  {
    outputs.out.value.clear();
    for(auto& val : inputs.in_i.ports)
      outputs.out.value.push_back(val.value);
  }
};
}
