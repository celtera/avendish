#pragma once
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <boost/container/static_vector.hpp>

#include <cstddef>
#include <span>
#include <string_view>

namespace halp
{
//! One component of a list element: a number in [min, max], or, when choices
//! is not empty, the index of one of them.
struct list_column
{
  std::string_view name{};
  double min{0.};
  double max{127.};
  double init{0.};
  bool integral{true};
  std::span<const std::string_view> choices{};
};

// A list of up to Capacity elements, edited row by row: hosts append, remove,
// duplicate and move rows, or take the whole list as text. Element is a number
// or an aggregate of numbers, one column per field, described by an optional
//   static constexpr std::array<halp::list_column, K> columns();
// and the count may be bounded with static constexpr int min_count() /
// max_count(). Init is the starting list.
// The value has a fixed capacity: the audio thread never allocates for it.
template <static_string lit, typename Element, std::size_t Capacity, Element... Init>
struct value_list
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
  enum class widget
  {
    value_list
  };
  using element_type = Element;
  static constexpr std::size_t capacity = Capacity;

  boost::container::static_vector<Element, Capacity> value{Init...};
};
}
