#pragma once
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

namespace ao
{
struct Spread
{
  halp_meta(name, "Spread array")
  halp_meta(c_name, "avnd_spread")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Control/Mappings")
  halp_meta(
      description,
      "Spread N inputs from a list: from [123, 'foo', 4.56] to 123 on inlet, 0, 'foo' "
      "on inlet 1 and 4.56 on inlet 2")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#spread")
  halp_meta(uuid, "cd6dc025-1d8e-4b73-868a-599df81667e4")

  struct
  {
    struct : halp::spinbox_i32<"Output count", halp::range{0, 1024, 1}>
    {
      static std::function<void(Spread&, int)> on_controller_interaction()
      {
        return [](Spread& object, int value) {
          object.outputs.out_i.request_port_resize(value);
        };
      }
    } controller;
    halp::val_port<"Input", std::vector<ossia::value>> in;
  } inputs;

  struct
  {
    halp::dynamic_port<halp::val_port<"Output {}", ossia::value>> out_i;
  } outputs;

  void operator()()
  {
    int N = std::min(outputs.out_i.ports.size(), inputs.in.value.size());
    for(int i = 0; i < N; i++)
    {
      auto& val = inputs.in.value[i];
      outputs.out_i.ports[i].value = std::move(val);
    }
  }
};
}
