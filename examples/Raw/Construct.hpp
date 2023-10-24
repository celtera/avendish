#pragma once
#include <variant>

namespace examples
{
/**
 * This example showcases more powerful handling of 
 * initialization as different objects can be constructed
 */

struct Object1
{
  struct
  {
    struct
    {
      float value;
    } out;
  } outputs;

  void operator()() { outputs.out.value = 1.0; }
};

struct Object2
{
  struct
  {
    struct
    {
      float value;
    } out1;
    struct
    {
      float value;
    } out2;
  } outputs;

  void operator()()
  {
    outputs.out1.value = 2.0;
    outputs.out2.value = 3.0;
  }
};

struct Construct
{
  static consteval auto name() { return "Init"; }
  static consteval auto c_name() { return "avnd_construct"; }
  static consteval auto uuid() { return "65f01367-9d0a-4769-8eda-61f7b11ae8d8"; }

  static std::variant<Object1, Object2> construct() { return Object1{}; }
  static std::variant<Object1, Object2> construct(int) { return Object2{}; }
};

}
