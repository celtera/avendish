#pragma once
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

namespace ao
{
struct Mux
{
  halp_meta(name, "Mux inlets")
  halp_meta(c_name, "avnd_mux")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Control/Mappings")
  halp_meta(
      description,
      "Pick input i from N inputs")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#mux")
  halp_meta(uuid, "deb6c556-afda-47c2-b545-d74c8e3958e4")

  struct
  {
    struct : halp::spinbox_i32<"Input count", halp::range{0, 1024, 2}>
    {
      static std::function<void(Mux&, int)> on_controller_interaction()
      {
        return [](Mux& object, int value) {
          object.inputs.in_i.request_port_resize(value);
        };
      }
    } controller;

    halp::spinbox_i32<"Current index", halp::range{0, 1024, 0}> index;

    // FIXME for this usecase we could have instead a placeholder type ?
    halp::dynamic_port<halp::val_port<"Input {}", ossia::value>> in_i;
  } inputs;

  struct
  {
    halp::val_port<"Output", ossia::value> out;
  } outputs;

  void operator()()
  {
    if(inputs.index.value >= 0 && inputs.index.value < inputs.in_i.ports.size())
      outputs.out.value = inputs.in_i.ports[inputs.index.value];
  }
};
}
