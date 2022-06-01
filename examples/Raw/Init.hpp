#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/meta.hpp>

#include <iostream>
#include <variant>

#if __has_include(<ranges>) && defined(__cpp_lib_ranges)
#include <ranges>
#define INPUT_RANGE std::ranges::input_range
#else
#define INPUT_RANGE
#endif

namespace examples
{
/**
 * This example showcases handling of input parameters,
 * e.g. the arguments passed in a Max or Pd object.
 */
struct Init
{
  static consteval auto name() { return "Init"; }
  static consteval auto c_name() { return "avnd_init"; }
  static consteval auto uuid() { return "1d9071aa-c314-45d6-b40b-422106f11773"; }

  struct
  {
    struct
    {
      static consteval auto name() { return "Float in"; }
      float value;
    } a;
  } inputs;

  struct
  {
    struct
    {
      static consteval auto name() { return "Float out"; }
      float value;
    } out;
  } outputs;

  using value = std::variant<float, std::string>;

  // Option A: a single constructor with fixed arguments
  void initialize(float a, std::string_view b)
  {
    std::cout << a << " ; " << b << std::endl;
    inputs.a.value = a;
  }

  // Option B: get a range with all the arguments.
  // Range will be akin to a vector<variant<>> of types that make sense.
  // An adapter to make things fit in specific types would be good.
  // void initialize(/* std::ranges::input_range */ auto range)
  // {
  //   for(auto elt : range)
  //   {
  //     std::visit([] (auto& e) {
  //       std::cout << e << std::endl;
  //     }, elt);
  //   }
  // }

  // Option C: overload set
  // static constexpr std::tuple initialize{
  //     [](Init& self, float a) { std::cout << "A: " << a << std::endl; },
  //     [](Init& self, float a, float b)
  //     { std::cout << "B: " << a << "; " << b << std::endl; },
  //     [](Init& self, std::string_view a) { std::cout << "C: " << a << std::endl; }};

// broken in GCC: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83258
#if (defined(__clang__) || defined(_MSC_VER))
  // halp_meta makes it a bit easier to define messages. Also, lambdas are possible.
  struct
  {
    struct
    {
      halp_meta(name, "generic_call");

      // Messages can also support a range-ish generic argument
      // (this is useful for hosts which support dynamic number of arguments)
      void operator()(Init& self, INPUT_RANGE auto range)
      {
        // value is a std::variant-like type
        for (auto& value : range)
        {
          // Use ADL here
          visit([](auto& e) { std::cout << e << std::endl; }, value);
        }
      }
    } generic_call;

    struct
    {
      halp_meta(name, "float_call");
      void operator()(float v)
      {
        std::cerr << "custom_call: " << v << std::endl;
      }
    } custom_call;

    // If the object type is passed as first argument, the obvious will happen.
    struct
    {
      halp_meta(name, "float_call_self");
      void operator()(Init& self, float v)
      {
        std::cerr << "value 2: " << v << ":" << self.inputs.a.value << std::endl;
      }
    } custom_call3;
  } messages;
#endif

  void operator()() { outputs.out.value = inputs.a.value; }
};
}
