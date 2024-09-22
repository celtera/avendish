#pragma once
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/messages.hpp>
#include <halp/meta.hpp>

#include <iostream>
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

      void update(CompleteMessageExample& a)
      {
        std::cerr << "Test1: " << value << std::endl;
        a.outputs.out_msg1(1.f, 10, "message1");
        a.outputs.out_msg2("heya", "message2");
      }
    } test1;

    struct : halp::val_port<"Test2", float>
    {
      // Class attribute: always goes to the first inlet
      // and does not create a new inlet
      halp_flag(class_attribute);

      void update(CompleteMessageExample& a)
      {
        std::cerr << "Test2: " << value << std::endl;
      }
    } test2;
    struct : halp::val_port<"Test3", float>
    {
      // Not a class attribute: will create a new inlet

      void update(CompleteMessageExample& a)
      {
        std::cerr << "Test3: " << value << std::endl;
      }
    } test3;
    struct : halp::val_port<"Test4", std::string>
    {
      // Not a class attribute: will create a new inlet

      void update(CompleteMessageExample& a)
      {
        std::cerr << "Test4: " << value << std::endl;
      }
    } test4;
    struct : halp::val_port<"Test5", std::vector<int>>
    {
      // Not a class attribute: will create a new inlet

      void update(CompleteMessageExample& a)
      {
        std::cerr << "Test5: [ ";
        for(auto v : value)
          std::cerr << v << ", ";
        std::cerr << "]\n";
      }
    } test5;
  } inputs;

  struct messages
  {
    struct
    {
      halp_meta(name, "m1");
      void operator()(int a, float b, std::string c)
      {
        std::cerr << "m1: " << a << " " << b << " " << c << "\n";
      }
    } m1;
  };

  struct
  {
    halp::callback<"some_symbol", float, int, std::string> out_msg1;
    halp::callback<"other_symbol", std::string, std::string> out_msg2;
  } outputs;

  void operator()()
  {
    std::cerr << "Test1: " << inputs.test1.value << "\n";
    std::cerr << "Test2: " << inputs.test2.value << "\n";
    std::cerr << "Test3: " << inputs.test3.value << "\n";
  }
};

}
