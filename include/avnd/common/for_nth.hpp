#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/coroutines.hpp>
#include <boost/pfr.hpp>

#include <utility>
#include <cassert>

namespace avnd
{

template <std::size_t N>
void for_nth(int k, auto&& f)
{
  [k]<std::size_t... Index>(std::index_sequence<Index...>, auto&& f)
  {
    ((void)(Index == k && (f.template operator()<Index>(), true)), ...);
  }
  (std::make_index_sequence<N>(), f);
}

template <class T, class F>
void for_each_field_ref(T&& value, F&& func)
{
  using namespace boost::pfr;
  using namespace boost::pfr::detail;
  constexpr std::size_t fields_count_val = fields_count<std::remove_reference_t<T>>();

  auto t = tie_as_tuple(value, size_t_<fields_count_val>{});

  [&]<std::size_t... I>(std::index_sequence<I...>)
  {
    (func(sequence_tuple::get<I>(t)), ...);
  }
  (make_index_sequence<fields_count_val>{});
}

template <class T, class F>
void for_each_field_ref(avnd::member_iterator<T>&& value, F&& func)
{
  for(auto& v : value) {
    for_each_field_ref(v, func);
  }
}
template <class T, class F>
void for_each_field_ref(avnd::member_iterator<T>& value, F&& func)
{
  for(auto& v : value) {
    for_each_field_ref(v, func);
  }
}
template <class T, class F>
void for_each_field_ref(const avnd::member_iterator<T>& value, F&& func)
{
  for(auto& v : value) {
    for_each_field_ref(v, func);
  }
}

constexpr int index_in_struct(const auto& s, auto member)
{
  int index = -1;
  int k = 0;
  
  avnd::for_each_field_ref(s, [&] (auto& m) {
    if constexpr (requires { bool(&m == &(s.*member)); })
    {
      if(&m == &(s.*member))
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
