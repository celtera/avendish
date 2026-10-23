#pragma once
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <array>
#include <string_view>

namespace halp
{
// Where each of the 12 degrees of an octave goes: value[d] is the degree it
// plays, 0 to 12 (12 being the octave above), or off to mute it. Hosts draw it
// as a matrix of input degrees (columns) by output degrees (rows).
template <static_string lit>
struct note_matrix
{
  static constexpr int off = -1;
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
  enum class widget
  {
    note_matrix
  };

  std::array<int, 12> value{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
};
}
