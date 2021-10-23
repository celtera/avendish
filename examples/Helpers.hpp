#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <helpers/controls.hpp>
#include <helpers/log.hpp>
#include <helpers/meta.hpp>

#include <cstdio>

// Sadly this example makes GCC segfault:
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=102990
namespace examples
{
/**
 * This examples shows how things can look with a small helper
 * library to abstract common use cases.
 */

template <typename C>
requires
    // Here we pass template arguments as a primitive form of dependency injection.
    // Out effect is saying: "I want to be passed configured with a type
    // holding a "logger_type" typedef
    // which will be something matching the logger concept.
    requires
{
  {
    typename C::logger_type { }
    } -> avnd::logger;
}
struct Helpers
{
  // $ is simply a macro that expands to a consteval function.
  // Hopefully C++ would use a similar syntax for reflexion.
  $(name, "Helpers")
  $(c_name, "avnd_helpers")
  $(uuid, "9d356a4b-a104-4b2a-a33e-c6828135d5c6")

  // We store our logger in the class to make things simpler.
  // no_unique_address makes sure that it stays a zero-memory-cost abstraction
  // if possible.
  [[no_unique_address]] typename C::logger_type logger;

  // Helper types for defining common cases of UI controls
  struct
  {
    avnd::slider_f32<"A"> a;
    avnd::knob_i32<"B", avnd::range{.min = -1000, .max = 1000, .init = 100}> b;
    avnd::toggle<"C", avnd::toggle_setup{.init = true}> c;
    avnd::lineedit<"D", "foo"> d;

    // The enum is a bit harder to do since we want to
    // define the enumerators and the matching strings in one go...
    // First argument after the name is the default (init) value.
    avnd__enum("Enum", Bar, Foo, Bar) e;
  } inputs;

  // Helpers for referring to local functions.
  // Ideally metaclasses would make that obsolete.
  void example(float x) { logger.log("example: {}", x); }

  struct
  {
    avnd::func_ref<"member", &Helpers<C>::example> my_message;
  } messages;

  struct
  {
    struct
    {
      float value;
    } out;
  } outputs;

  void operator()()
  {
    if (inputs.e.value == inputs.e.Foo)
    {
      outputs.out.value = inputs.a.value + inputs.b.value;
    }
  }
};
}
