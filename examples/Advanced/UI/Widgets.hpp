#pragma once
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/messages.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>

/* SPDX-License-Identifier: GPL-3.0-or-later */

namespace uo
{
struct Impulse
{
  halp_meta(name, "Impulse")
  halp_meta(c_name, "avnd_impulse")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Send an impulse whenever there is an input message")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#impulse")
  halp_meta(uuid, "161722c3-a2a4-4958-8510-7fde9160bf44")

  struct
  {
    halp::toggle<"Skip false-y values"> skip_falsey;
  } inputs;

  struct truey_vis
  {
    bool operator()() { return false; }
    bool operator()(ossia::impulse) { return true; }
    bool operator()(int v) { return v != 0; }
    bool operator()(float v) { return v != 0; }
    bool operator()(ossia::vec2f) { return true; }
    bool operator()(ossia::vec3f) { return true; }
    bool operator()(ossia::vec4f) { return true; }
    bool operator()(const std::string& v) { return !v.empty(); }
    bool operator()(const std::vector<ossia::value>& v) { return !v.empty(); }
    bool operator()(const ossia::value_map_type& v) { return !v.empty(); }
  };

  struct messages
  {
    struct
    {
      static consteval auto name() { return "Message"; }
      void operator()(Impulse& self, const ossia::value& v)
      {
        if(!self.inputs.skip_falsey || v.apply(truey_vis{}))
          self.outputs.out();
      }
    } functor;
  };

  struct
  {
    halp::callback<"Impulse"> out;
  } outputs;

  void operator()() noexcept { }
};

struct Integer
{
  halp_meta(name, "Integer")
  halp_meta(c_name, "avnd_integer")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Send an integer whenever there is an input message")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#integer")
  halp_meta(uuid, "e661b8c1-672e-4350-a46e-9466917c5422")

  struct
  {
    struct : halp::spinbox_i32<"Value", halp::free_range_max<int>>
    {
    } v;
  } inputs;

  struct
  {
    halp::callback<"Integer", int> out;
  } outputs;

  void operator()() noexcept { outputs.out(inputs.v.value); }
};

struct Float
{
  halp_meta(name, "Float")
  halp_meta(c_name, "avnd_float")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Send a float whenever there is an input message")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#float")
  halp_meta(uuid, "ee3a50c0-a202-4f51-a26d-be57a939997d")

  struct
  {
    struct : halp::spinbox_f32<"Value", halp::free_range_max<float>>
    {
    } v;
  } inputs;

  struct
  {
    halp::callback<"Out", float> out;
  } outputs;

  void operator()() noexcept { outputs.out(inputs.v.value); }
};

struct String
{
  halp_meta(name, "String")
  halp_meta(c_name, "avnd_string")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Send a string whenever there is an input message")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#string")
  halp_meta(uuid, "4770e6cd-53a8-4943-b13d-7cb55af429b3")

  struct
  {
    struct : halp::lineedit<"Value", "text">
    {
    } v;
  } inputs;

  struct
  {
    halp::callback<"Out", std::string> out;
  } outputs;

  void operator()() noexcept { }
};

class Knob
{
public:
  halp_meta(name, "Knob")
  halp_meta(c_name, "avnd_ui_knob")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Basic UI knob")
  halp_meta(uuid, "209b71d0-d7ee-49d1-ba79-557e32ef4888")

  struct
  {
    struct : halp::knob_f32<"Value", halp::range{0., 1., 0.5}>
    {
    } v;
  } inputs;

  struct
  {
    halp::val_port<"Output", float> out;
  } outputs;

  void operator()() noexcept { outputs.out = inputs.v.value; }
};
}
