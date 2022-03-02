#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/helpers/messages.hpp>
#include <avnd/helpers/meta.hpp>
#include <avnd/helpers/log.hpp>

#include <cstdio>
// Note: we use a generic logger abstraction here, so
// that we can use e.g. Pd / Max's post in their case, and
// printf / fmt::print otherwise.
// See Logger.hpp for more detail on how that works !

namespace examples::helpers
{
inline void free_example()
{
  printf("free_example A\n");
  fflush(stdout);
}

template<typename C>
inline void free_template_example()
{
  using logger = typename C::logger_type;
  logger{}.log("free_example");
}

// See Logger.hpp
template <typename C>
requires
    avnd::has_logger<C>
struct Messages
{
  $(name, "Message helpers")
  $(c_name, "avnd_helpers_messages")
  $(uuid, "0029b546-cddb-49b1-9c99-c659b16e58eb")

  [[no_unique_address]] typename C::logger_type logger;

  void example()
  {
    logger.log("example");
  }

  void example2(float x)
  {
    logger.log("example2: {}", x);
  }

  template <typename U>
  void example3(U x)
  {
    logger.log("example3: {}", x);
  }

  avnd_start_messages(Messages)
    avnd_mem_fun(example)
    avnd_mem_fun(example2)
    avnd_mem_fun_t(example3, <int>)
    avnd_mem_fun_t(example3, <float>)
    avnd_mem_fun_t(example3, <const char*>)
    avnd_free_fun(free_example)
    avnd_free_fun_t(free_template_example, <C>)
    avnd_lambda(my_lambda, [] (Messages& self) { puts("lambda"); })

    // General case:
    avnd::func_ref<"my_message", &Messages::example> m_my_message;
  avnd_end_messages
};
}
