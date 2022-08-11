#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <array>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace examples
{
/**
 * This example showcases the "message" behaviour of using std::optional<foo> value.
 * The inputs will only be set by the host environment whenever a message is received.
 * The outputs will only send a message whenever the processor sets the optional,
 * and are resetted before the next tick.
 */
struct OptionalTest
{
  static consteval auto name() { return "Optional Input"; }
  static consteval auto c_name() { return "avnd_optional"; }
  static consteval auto uuid() { return "d7cb78dd-46bc-4b49-8f23-262cb0b457b7"; }

  struct TestAggregate
  {
    int a, b;
    float c;
    struct
    {
      std::string d;
    } sub;
  };

  template <typename T>
  using array = std::array<T, 7>;

  using variant = std::variant<int, bool, std::string>;

#define port(type, var)                            \
  struct                                           \
  {                                                \
    static consteval auto name() { return #type; } \
    std::optional<type> value;                     \
  } var
  struct
  {
    port(variant, in_var);
    port(int, in_int);
    port(float, in_float);
    port(double, in_dbl);
    port(bool, in_bool);
    port(char, in_char);
    port(TestAggregate, in_aggreg);
    port(std::string, in_str);
    port(std::vector<int>, in_vecint);
    port(std::vector<float>, in_vecfloat);
    port(std::vector<double>, in_vecdbl);
    port(std::vector<bool>, in_vecbool);
    port(std::vector<char>, in_vecchar);
    port(std::vector<std::string>, in_vecstr);
    port(std::vector<TestAggregate>, in_vecaggreg);
    port(array<int>, in_arrint);
    port(array<float>, in_arrfloat);
    port(array<double>, in_arrdbl);
    port(array<bool>, in_arrbool);
    port(array<char>, in_arrchar);
    port(array<std::string>, in_arrstr);
    port(array<TestAggregate>, in_arraggreg);
  } inputs;

  struct
  {
    port(variant, out_var);
    port(int, out_int);
    port(float, out_float);
    port(double, out_dbl);
    port(bool, out_bool);
    port(char, out_char);
    port(TestAggregate, out_aggreg);
    port(std::string, out_str);
    port(std::vector<int>, out_vecint);
    port(std::vector<float>, out_vecfloat);
    port(std::vector<double>, out_vecdbl);
    port(std::vector<bool>, out_vecbool);
    port(std::vector<char>, out_vecchar);
    port(std::vector<std::string>, out_vecstr);
    port(std::vector<TestAggregate>, out_vecaggreg);
    port(array<int>, out_arrint);
    port(array<float>, out_arrfloat);
    port(array<double>, out_arrdbl);
    port(array<bool>, out_arrbool);
    port(array<char>, out_arrchar);
    port(array<std::string>, out_arrstr);
    port(array<TestAggregate>, out_arraggreg);
  } outputs;
#undef port

  void operator()()
  {
    outputs.out_var.value = inputs.in_var.value;
    outputs.out_int.value = inputs.in_int.value;
    outputs.out_float.value = inputs.in_float.value;
    outputs.out_dbl.value = inputs.in_dbl.value;
    outputs.out_bool.value = inputs.in_bool.value;
    outputs.out_char.value = inputs.in_char.value;
    outputs.out_aggreg.value = inputs.in_aggreg.value;
    outputs.out_str.value = inputs.in_str.value;
    outputs.out_vecint.value = inputs.in_vecint.value;
    outputs.out_vecfloat.value = inputs.in_vecfloat.value;
    outputs.out_vecdbl.value = inputs.in_vecdbl.value;
    outputs.out_vecbool.value = inputs.in_vecbool.value;
    outputs.out_vecchar.value = inputs.in_vecchar.value;
    outputs.out_vecstr.value = inputs.in_vecstr.value;
    outputs.out_vecaggreg.value = inputs.in_vecaggreg.value;
    outputs.out_arrint.value = inputs.in_arrint.value;
    outputs.out_arrfloat.value = inputs.in_arrfloat.value;
    outputs.out_arrdbl.value = inputs.in_arrdbl.value;
    outputs.out_arrbool.value = inputs.in_arrbool.value;
    outputs.out_arrchar.value = inputs.in_arrchar.value;
    outputs.out_arrstr.value = inputs.in_arrstr.value;
    outputs.out_arraggreg.value = inputs.in_arraggreg.value;
  }
};
}
