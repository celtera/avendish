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

struct Vec2i
{
  halp_meta(name, "Vec2i")
  halp_meta(c_name, "avnd_vec2i")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Send an integer pair whenever there is an input message")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#vec")
  halp_meta(uuid, "4d20454d-de79-4be9-8b50-d288f3ecc33c")

  struct
  {
    halp::spinbox_i32<"X", halp::free_range_max<int>> v1;
    halp::spinbox_i32<"Y", halp::free_range_max<int>> v2;
  } inputs;

  struct
  {
    halp::callback<"Pair", std::array<int, 2>> out;
  } outputs;

  void operator()() noexcept
  {
    outputs.out(std::array<int, 2>{inputs.v1.value, inputs.v2.value});
  }
};

struct Vec3i
{
  halp_meta(name, "Vec3i")
  halp_meta(c_name, "avnd_vec3i")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Send a Vec3i whenever there is an input message")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#vec")
  halp_meta(uuid, "f8a894c7-9f8e-42f3-9a6e-ccee1ab5b20d")

  struct
  {
    halp::spinbox_i32<"X", halp::free_range_max<int>> v1;
    halp::spinbox_i32<"Y", halp::free_range_max<int>> v2;
    halp::spinbox_i32<"Z", halp::free_range_max<int>> v3;
  } inputs;

  struct
  {
    halp::callback<"Pair", std::array<int, 3>> out;
  } outputs;

  void operator()() noexcept
  {
    outputs.out(std::array<int, 3>{inputs.v1.value, inputs.v2.value, inputs.v3.value});
  }
};
struct Vec4i
{
  halp_meta(name, "Vec4i")
  halp_meta(c_name, "avnd_vec4i")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Send a Vec4i whenever there is an input message")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#vec")
  halp_meta(uuid, "b2e60c29-2b9b-41ea-9721-5aa3b994a82f")

  struct
  {
    halp::spinbox_i32<"X", halp::free_range_max<int>> v1;
    halp::spinbox_i32<"Y", halp::free_range_max<int>> v2;
    halp::spinbox_i32<"Z", halp::free_range_max<int>> v3;
    halp::spinbox_i32<"W", halp::free_range_max<int>> v4;
  } inputs;

  struct
  {
    halp::callback<"Pair", std::array<int, 4>> out;
  } outputs;

  void operator()() noexcept
  {
    outputs.out(
        std::array<int, 4>{
            inputs.v1.value, inputs.v2.value, inputs.v3.value, inputs.v4.value});
  }
};

struct Vec2f
{
  halp_meta(name, "Vec2f")
  halp_meta(c_name, "avnd_vec2f")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Send a float pair whenever there is an input message")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#vec")
  halp_meta(uuid, "d4e4574c-02c0-4b3e-9409-833837ff0921")

  struct
  {
    halp::spinbox_f32<"X", halp::free_range_max<float>> v1;
    halp::spinbox_f32<"Y", halp::free_range_max<float>> v2;
  } inputs;

  struct
  {
    halp::callback<"Pair", std::array<float, 2>> out;
  } outputs;

  void operator()() noexcept
  {
    outputs.out(std::array<float, 2>{inputs.v1.value, inputs.v2.value});
  }
};

struct Vec3f
{
  halp_meta(name, "Vec3f")
  halp_meta(c_name, "avnd_vec3f")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Send a Vec3f whenever there is an input message")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#vec")
  halp_meta(uuid, "6e74b962-b102-4d2d-b6f1-850917e8c747")

  struct
  {
    halp::spinbox_f32<"X", halp::free_range_max<float>> v1;
    halp::spinbox_f32<"Y", halp::free_range_max<float>> v2;
    halp::spinbox_f32<"Z", halp::free_range_max<float>> v3;
  } inputs;

  struct
  {
    halp::callback<"Pair", std::array<float, 3>> out;
  } outputs;

  void operator()() noexcept
  {
    outputs.out(std::array<float, 3>{inputs.v1.value, inputs.v2.value, inputs.v3.value});
  }
};
struct Vec4f
{
  halp_meta(name, "Vec4f")
  halp_meta(c_name, "avnd_vec4f")
  halp_meta(category, "Control/Basic")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Send a Vec4f whenever there is an input message")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/mapping-utilities.html#vec")
  halp_meta(uuid, "5ccd5c1f-5614-4c98-9c5b-f0d7e3dfe49f")

  struct
  {
    halp::spinbox_f32<"X", halp::free_range_max<float>> v1;
    halp::spinbox_f32<"Y", halp::free_range_max<float>> v2;
    halp::spinbox_f32<"Z", halp::free_range_max<float>> v3;
    halp::spinbox_f32<"W", halp::free_range_max<float>> v4;
  } inputs;

  struct
  {
    halp::callback<"Pair", std::array<float, 4>> out;
  } outputs;

  void operator()() noexcept
  {
    outputs.out(
        std::array<float, 4>{
            inputs.v1.value, inputs.v2.value, inputs.v3.value, inputs.v4.value});
  }
};
}
