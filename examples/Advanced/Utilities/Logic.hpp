#pragma once
#include "Math.hpp"

#include <optional>

namespace avnd::blocks
{

struct Choice : Triop
{
  static consteval auto name() { return "Choice"; }
  static consteval auto c_name() { return "choice"; }
  static consteval auto uuid() { return "3d51274b-0dac-4b9f-9867-d3d303468ebb"; }

  void operator()()
  {
    if(inputs.c.value != 0.)
      outputs.a.value = inputs.a.value;
    else
      outputs.a.value = inputs.b.value;
  }
};

struct Spigot : Base
{
  struct
  {
    struct
    {
      static consteval auto name() { return "Input 1"; }
      double value{};
    } a;
    struct
    {
      static consteval auto name() { return "Input 2"; }
      double value{};
    } b;
  } inputs;

  struct
  {
    struct
    {
      static consteval auto name() { return "Output 1"; }
      std::optional<double> value{};
    } a;
  } outputs;

  static consteval auto name() { return "Spigot"; }
  static consteval auto c_name() { return "spigot"; }
  static consteval auto uuid() { return "b7dfb274-b443-4121-8398-21b34446044d"; }
  void operator()()
  {
    if(inputs.b.value != 0.)
      outputs.a.value = inputs.a.value;
    else
      outputs.a.value = std::nullopt;
  }
};

struct Stutter : Base
{

  static consteval auto name() { return "Stutter"; }
  static consteval auto c_name() { return "stutter"; }
  static consteval auto uuid() { return "073a983b-abcb-42f1-afa4-49172a044d6c"; }

  struct
  {
    struct
    {
      static consteval auto name() { return "Input 1"; }
      double value{};
    } a;
    struct
    {
      static consteval auto name() { return "Input 2"; }
      double value{};
    } b;
  } inputs;

  struct
  {
    struct
    {
      static consteval auto name() { return "Output 1"; }
      std::optional<double> value{};
    } a;
  } outputs;

  int counter = 0;
  void operator()()
  {

    if(inputs.b.value != 0.)
      outputs.a.value = inputs.a.value;
    else
      outputs.a.value = std::nullopt;
  }
};

}
