#pragma once

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/messages.hpp>
#include <halp/meta.hpp>

#include <fmt/format.h>
#include <fmt/std.h>
#include <fmt/ranges.h>
#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace examples::helpers
{
struct CompleteMessageExample
{
  halp_meta(name, "CompleteMessageExample")
  halp_meta(c_name, "avnd_complete_message_example")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Examples")
  halp_meta(description, "Test all the Max & Pd features at once")
  halp_meta(uuid, "ecbf8e41-5596-4d13-8407-6b962a02fa54")

  struct
  {
    // Pd:
    // - first inlet will receive either:
    //   * Numbers directly
    //   * The message [ Test1 123 >
    //   * The message [ test1 45 >
    struct : halp::val_port<"Test1", int>
    {
      // Class attribute: always goes to the first inlet
      // and does not create a new inlet
      halp_flag(class_attribute);

      // self is the object instance
      void update(CompleteMessageExample& self)
      {
        std::cerr << "Test1: " << value << '\n';
        self.outputs.out_msg1(1.f, 10, "message1");
        self.outputs.out_msg2("heya", "message2");
      }
    } test1;

    // Class attribute: always goes to the first inlet
    // and does not create a new inlet
    struct : halp::val_port<"Test2", float>
    {
      halp_flag(class_attribute);

      void update(CompleteMessageExample& self)
      {
        std::cerr << "Test2: " << value << '\n';
      }
    } test2;

    // Not a class attribute: will create a new inlet (second inlet)
    struct : halp::val_port<"Test3", float>
    {
      void update(CompleteMessageExample& self)
      {
        std::cerr << "Test3: " << value << '\n';
      }
    } test3;

    // Not a class attribute: will create a new inlet (third inlet)
    struct : halp::val_port<"Test4", std::string>
    {
      void update(CompleteMessageExample& self)
      {
        std::cerr << "Test4: " << value << '\n';
      }
    } test4;

    // Not a class attribute: will create a new inlet (fourth inlet)
    struct : halp::val_port<"Test5", std::vector<int>>
    {
      void update(CompleteMessageExample& self)
      {
        std::cerr << "Test5: [ ";
        for(auto v : value)
          std::cerr << v << ", ";
        std::cerr << "]\n";
      }
    } test5;

    // Not a class attribute: will create a new inlet (fifth inlet)
    halp::hslider_f32<"Slider", halp::range{-1., 1., 0.5}> slider;
  } inputs;

  // Messages also always go to the first inlet in Max / Pd.
  // In ossia they create new ports.
  struct messages
  {
    // A basic message.
    // Hopefully in the future with C++26 reflection we can just define straight member functions here.
    struct
    {
      halp_meta(name, "m1");
      void operator()(int a, float b, std::string c)
      {
        std::cerr << "m1: " << a << " " << b << " " << c << "\n";
      }
    } m1;

    // This message takes the object as first argument.
    struct
    {
      halp_meta(name, "m2");
      void operator()(CompleteMessageExample& self, int a, float b, std::string c)
      {
        std::cerr << "m2: " << a << " " << b << " " << c << "\n";
      }
    } m2;
  };

  // Used in outputs
  struct some_object
  {
    int x;
    std::string v;
  };
  struct some_object_named
  {
    int x;
    std::string v;
    halp_field_names(x, v);
  };

  struct
  {
    // Case 1: outputting a basic value
    // pd: will output [float 123>
    // max: will output [long 123>
    halp::val_port<"out_0", int> out_int;

    // Case 2: outputting a basic tuple
    // pd: will output [list 123 456 789>
    // max: will output [list 123 456 789>
    halp::val_port<"out_1", std::array<float, 3>> out_vec3;

    // Case 2: outputting an object without names
    // Note that the object can be defined elsewhere.
    // Only rules are no specific constructors / destructors, only aggregate types.
    // pd: will output [list 123 foo>
    // max: will output [list 123 foo>
    halp::val_port<"out_2", some_object> out_obj1;

    // Case 3: outputting an object with names
    // pd: will output [list 123 foo>
    // max: will output [dict x:123 v:foo>
    halp::val_port<"out_3", some_object_named> out_obj2;

    // Case 4: outputting messages
    // pd: will output [ list 0.f 0 foo >
    halp::callback<"cb1", float, int, std::string> out_msg1;

    // pd: will output [ symbol2 foo bar >
    struct : halp::callback<"cb2", std::string, std::string>
    {
      halp_meta(symbol, "symbol2")
      halp_meta(c_name, "symbol2") // equivalent
    } out_msg2;

    // pd: will output [ list foo bar ... >
    halp::callback<"cb3", std::vector<std::string>> out_msg_list;

    // pd: will output [ symbol4 foo bar ... >
    // note: in C++29 it looks like we will finally be able
    // to have custom parseable attributes:
    // [[symbol: "foo"]], etc...
    struct : halp::callback<"cb4", std::vector<std::string>>
    {
      halp_meta(symbol, "symbol2")
      halp_meta(c_name, "symbol2") // equivalent
    } out_msg_listsym;

    // pd: will use whatever is in the first string as selector for the message
    struct : halp::callback<"cb5", std::string, std::vector<std::string>>
    {
      halp_flag(dynamic_symbol);
    } out_msg_dynsym;
  } outputs;

  void operator()()
  {
    std::cerr << "Test1: " << inputs.test1.value << " ; ";
    std::cerr << "Test2: " << inputs.test2.value << " ; ";
    std::cerr << "Test3: " << inputs.test3.value << " ; ";
    std::cerr << "Test4: " << inputs.test4.value << " ; ";
    std::cerr << "Test5: " << fmt::format("{}", inputs.test5.value) << " ; ";
    std::cerr << "Test6: " << inputs.slider.value << "\n\n";
    outputs.out_int++;
    outputs.out_vec3 = std::array{4.f, 5.f, 6.f};
    outputs.out_obj1 = some_object{outputs.out_int, "hello"};
    outputs.out_obj2 = some_object_named{outputs.out_int, "bye"};
    outputs.out_msg1(inputs.slider.value, outputs.out_int, "from bang");
    outputs.out_msg2("text1", "text2");
    outputs.out_msg_list(std::vector<std::string>{"text1", "text2", "text3"});
    std::vector<std::string> v{"a", "b", "c", "d"};
    outputs.out_msg_listsym(v);
    outputs.out_msg_dynsym("random_sym", std::vector<std::string>{"x", "y", "z", "w"});
  }
};

}
