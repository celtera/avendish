#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/coroutines.hpp>
#include <avnd/common/aggregates.hpp>
#include <avnd/common/index.hpp>
#include <avnd/concepts/generic.hpp>

#include <cassert>
#include <utility>

namespace avnd
{

template <std::size_t N>
constexpr void for_nth(int k, auto&& f)
{
  [k]<std::size_t... Index>(std::index_sequence<Index...>, auto&& f)
  {
    ((void)(Index == k && (f.template operator()<Index>(), true)), ...);
  }
  (std::make_index_sequence<N>(), f);
}

template <class T, class F>
requires (!avnd::vector_ish<std::decay_t<T>>)
constexpr void for_each_field_ref(T&& value, F&& func)
{
  static_assert(!requires { value.size(); });
#if AVND_USE_BOOST_PFR
  using namespace pfr;
  using namespace pfr::detail;
  constexpr std::size_t fields_count_val = fields_count<std::remove_reference_t<T>>();

  auto t = tie_as_tuple(value, size_t_<fields_count_val>{});

  [&]<std::size_t... I>(std::index_sequence<I...>)
  {
    (func(sequence_tuple::get<I>(t)), ...);
  }
  (make_index_sequence<fields_count_val>{});
#else
  auto&& [...elts] = value;
  (func(elts), ...);
#endif
}
template <class T, class F>
requires (!avnd::vector_ish<T>)
constexpr void for_each_field_ref_n(T&& value, F&& func)
{
#if AVND_USE_BOOST_PFR
  using namespace pfr;
  using namespace pfr::detail;
  constexpr std::size_t fields_count_val = fields_count<std::remove_reference_t<T>>();

  auto t = tie_as_tuple(value, size_t_<fields_count_val>{});

  [&]<std::size_t... I>(std::index_sequence<I...>)
  {
    (func(sequence_tuple::get<I>(t), avnd::field_index<I>{}), ...);
  }
  (make_index_sequence<fields_count_val>{});
#else
  auto&& [...elts] = value;
  (func(elts), ...);
#endif
}

template <class T, class F>
void for_each_field_ref(avnd::member_iterator<T>&& value, F&& func)
{
  for (auto& v : value)
  {
    for_each_field_ref(v, func);
  }
}
template <class T, class F>
void for_each_field_ref(avnd::member_iterator<T>& value, F&& func)
{
  for (auto& v : value)
  {
    for_each_field_ref(v, func);
  }
}
template <class T, class F>
void for_each_field_ref(const avnd::member_iterator<T>& value, F&& func)
{
  for (auto& v : value)
  {
    for_each_field_ref(v, func);
  }
}

template <typename T, class F>
requires avnd::vector_ish<std::decay_t<T>>
void for_each_field_ref(T&& value, F&& func)
{
  for (auto& v : value)
  {
    func(v);
  }
}

constexpr int index_in_struct(const auto& s, auto member)
{
  int index = -1;
  int k = 0;

  avnd::for_each_field_ref(
      s,
      [&](auto& m)
      {
        if constexpr (requires { bool(&m == &(s.*member)); })
        {
          if (&m == &(s.*member))
          {
            index = k;
          }
        }
        ++k;
      });
  assert(index >= 0);
  return index;
}
}
