#pragma once
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

namespace ao
{
struct Demux
{
  halp_meta(name, "Demux outlets")
  halp_meta(c_name, "avnd_demux")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Control/Mappings")
  halp_meta(
      description,
      "Send input i to an outlet out of N")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#demux")
  halp_meta(uuid, "37ba8700-a924-4364-b761-a14cc036cef3")

  struct
  {
    struct : halp::spinbox_i32<"Output count", halp::range{0, 1024, 1}>
    {
      static std::function<void(Demux&, int)> on_controller_interaction()
      {
        return [](Demux& object, int value) {
          object.outputs.out_i.request_port_resize(value);
        };
      }
    } controller;
    halp::spinbox_i32<"Current index", halp::range{0, 1024, 0}> index;
    halp::val_port<"Input", ossia::value> in;
  } inputs;

  struct
  {
    halp::dynamic_port<halp::val_port<"Output {}", ossia::value>> out_i;
  } outputs;

  void operator()()
  {
    if(inputs.index.value >= 0 && inputs.index.value < outputs.out_i.ports.size())
    {
      outputs.out_i.ports[inputs.index.value].value = std::move(inputs.in.value);
    }
  }
};
}
