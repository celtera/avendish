#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cstdlib>

#include <type_traits>

#define AVND_STATIC_TODO(T) static_assert(std::is_void_v<T>, "TODO !");
#define AVND_ERROR(T, Message) static_assert(std::is_void_v<T>, Message);
#define AVND_TODO \
  do              \
  {               \
    std::abort(); \
  } while(0)
