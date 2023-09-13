#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <iostream>

namespace examples::helpers
{

/**
 */
struct Attributes
{
  halp_meta(name, "Attributes")
  halp_meta(c_name, "avnd_attributes")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(category, "Devices")
  halp_meta(description, "Test class attribute inputs")
  halp_meta(uuid, "8a05bbf1-e8a5-44ec-8f37-b5e1d046bed8")
  struct
  {
    struct : halp::val_port<"Index", int> {
      halp_flag(class_attribute);

      void update(Attributes& a) {
        std::cerr << "Index: " << value << std::endl;
      }
    } device_index;

    struct: halp::val_port<"Serial", std::string> {
      halp_flag(class_attribute);
      void update(Attributes& a) {
        std::cerr << "Serial: " << value << std::endl;
      }
    } device_serial;

    struct: halp::val_port<"Foo", int> {
      void update(Attributes& a) {
        std::cerr << "Foo: " << value << std::endl;
      }
    } foo;

    struct: halp::val_port<"Connected", bool> {
      halp_flag(class_attribute);
      void update(Attributes& a) {
        std::cerr << "Connected: " << value << std::endl;
      }
    } connected;

    struct: halp::val_port<"Bar", int> {
      void update(Attributes& a) {
        std::cerr << "Bar: " << value << std::endl;
      }
    } bar;
  } inputs;

  struct
  {
  } outputs;

  void operator()()
  {

  }
};

}
