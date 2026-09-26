#pragma once
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/messages.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/math.hpp>

#include <optional>

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
  // Set when a message or a bang asks for the count to go out. The output is
  // written by operator(): the binding clears the outputs after the controls'
  // update() ran and before operator(), and the inspector's buttons run
  // update() between two ticks.
  bool pending{};

  //! The count as the Mode shapes it: clipped, wrapped or folded to Max.
  int64_t shaped() const noexcept
  {
    const auto max = (int64_t)inputs.max.value;
    switch(inputs.ceil)
    {
      default:
      case Free:
        return count;
      case Clip:
        return std::min(count, max);
      case Wrap:
        return ossia::wrap(count, (int64_t)0, max);
      case Fold:
        return ossia::fold(count, (int64_t)0, max);
    }
  }

  void send() { pending = true; }

  void increase()
  {
    ++count;
    if(inputs.when == OnInput)
      send();
    if(count >= inputs.max.value)
      outputs.ceiling();
  }

  void bang()
  {
    send();
    if(count >= inputs.max.value)
      outputs.ceiling();
  }

  void reset()
  {
    count = 0;
    if(inputs.when != Manually)
      send();
  }

  void operator()()
  {
    if(pending || inputs.when == EveryTick)
      outputs.count.value = int(shaped());
    else
      outputs.count.value.reset();
    pending = false;
  }

  enum Mode
  {
    Free,
    Clip,
    Wrap,
    Fold
  };

  enum OutputMode
  {
    EveryTick,
    OnInput,
    Manually
  };

  struct
  {
    halp::enum_t<Mode, "Mode"> ceil;
    halp::spinbox_i32<"Max", halp::range{0, std::numeric_limits<int>::max(), 100}> max;
    struct : halp::impulse_button<"Output">
    {
      void update(Counter& self) { self.bang(); }
    } output;
    struct : halp::impulse_button<"Reset">
    {
      void update(Counter& self) { self.reset(); }
    } reset;
    //! Every tick, on each message, or only on the Output bang.
    halp::combobox_t<"Send", OutputMode> when;
  } inputs;

  struct messages
  {
    using parent_type = Counter;
    halp::func_ref<"Increase", &Counter::increase> m;
  };

  struct
  {
    halp::val_port<"Count", std::optional<int>> count;
    halp::callback<"Ceiling"> ceiling;
  } outputs;
};
}
