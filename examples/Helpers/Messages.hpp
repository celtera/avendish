#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <helpers/messages.hpp>
#include <helpers/meta.hpp>

#include <cstdio>

namespace examples::helpers
{
inline void free_example()
{
  puts("free_example");
}

struct Messages
{
  $(name, "Message helpers")
  $(c_name, "avnd_helpers_messages")
  $(uuid, "0029b546-cddb-49b1-9c99-c659b16e58eb")

  void example()
  {
    puts("example");
  }

  void example2(float x)
  {
    printf("example2: %f", x);
  }

  avnd_start_messages(Messages)
    avnd_mem_fun(example)
    avnd_mem_fun(example2)
    avnd_free_fun(free_example)
    avnd_lambda(my_lambda, [] (Messages& self) { puts("lambda"); })
  avnd_end_messages
};
}
