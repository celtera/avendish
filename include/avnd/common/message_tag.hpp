#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Annotation tags for declaring messages as member functions under reflection:
//   [[=halp::message]]            void increment(int by);  // name = "increment"
//   [[=halp::message_named("!")]] void bang();             // name = "!"
// Re-exported by halp as halp::message / halp::message_named.

#include <cstddef>
#include <string_view>

namespace avnd
{
// Marker: [[=halp::message]] — the message name is the function identifier.
struct message_annotation
{
};

// Explicit name: [[=halp::message_named("foo")]]
template <std::size_t N>
struct named_message_annotation
{
  char storage[N]{};
  consteval named_message_annotation(const char (&s)[N]) noexcept
  {
    for(std::size_t i = 0; i < N; ++i)
      storage[i] = s[i];
  }
  constexpr std::string_view name() const noexcept { return {storage, N - 1}; }
};
}
