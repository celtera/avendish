#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/log.hpp>
#include <halp/messages.hpp>
#include <halp/meta.hpp>

// Sadly this example makes GCC segfault:
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=102990
namespace examples::helpers
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
    halp::has_logger<C>
struct Logger
{
  // halp_meta is simply a macro that expands to a consteval function.
  // Hopefully C++ would use a similar syntax for reflexion.
  halp_meta(name, "Helpers")
  halp_meta(c_name, "avnd_helpers_logger")
  halp_meta(uuid, "3a646521-48f4-429b-a2b1-d67beb0d65cf")

  // We store our logger in the class to make things simpler.
  // no_unique_address makes sure that it stays a zero-memory-cost abstraction
  // if possible.
  [[no_unique_address]] typename C::logger_type logger;

  // Helpers for referring to local functions.
  // Ideally metaclasses would make that obsolete.
  void example(float x)
  {
    logger.trace("example: {}", x);
    logger.info("example: {}", x);
    logger.debug("example: {}", x);
    logger.warn("example: {}", x);
    logger.error("example: {}", x);
    logger.critical("example: {}", x);
  }

  struct
  {
    halp::func_ref<"member", &Logger<C>::example> my_message;
  } messages;
};
}
