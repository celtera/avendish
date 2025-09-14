#pragma once
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/messages.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/math.hpp>

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace examples
{
struct Counter
{
  halp_meta(name, "Counter")
  halp_meta(c_name, "avnd_counter")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Count the number of messages received")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#counter")
  halp_meta(uuid, "acdc0a7e-676f-462c-b46d-c6cd99fa74a2")

  int64_t count{};
  void increase()
  {
    ++count;
    switch(inputs.ceil)
    {
      default:
      case Free:
        outputs.count = count;
        break;
      case Clip:
        outputs.count = std::min(count, (int64_t)inputs.max.value);
        break;
      case Wrap:
        outputs.count = ossia::wrap(count, (int64_t)0, (int64_t)inputs.max.value);
        break;
      case Fold:
        outputs.count = ossia::fold(count, (int64_t)0, (int64_t)inputs.max.value);
        break;
    }
    if(count >= inputs.max.value)
      outputs.ceiling();
  }

  void bang()
  {
    outputs.count = count;
    if(count >= inputs.max.value)
      outputs.ceiling();
  }

  enum Mode
  {
    Free,
    Clip,
    Wrap,
    Fold
  };

  struct
  {
    halp::enum_t<Mode, "Mode"> ceil;
    halp::spinbox_i32<"Max", halp::range{0, std::numeric_limits<int>::max(), 100}> max;
    struct : halp::impulse_button<"Output">
    {
      void update(Counter& self) { self.bang(); }
    } output;
  } inputs;

  struct messages
  {
    using parent_type = Counter;
    halp::func_ref<"Increase", &Counter::increase> m;
  };

  struct
  {
    halp::val_port<"Count", int> count;
    halp::callback<"Ceiling"> ceiling;
  } outputs;
};
}
