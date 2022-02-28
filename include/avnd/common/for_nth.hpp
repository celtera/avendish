#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <utility>
#include <boost/pfr.hpp>

namespace avnd
{

template <std::size_t N>
void for_nth(int k, auto&& f)
{
  [k]<std::size_t... Index>(
      std::index_sequence<Index...>, auto&& f)
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

  [&] <std::size_t... I> (std::index_sequence<I...>){
    (func(sequence_tuple::get<I>(t)), ...);
  }(make_index_sequence<fields_count_val>{});
}

}
