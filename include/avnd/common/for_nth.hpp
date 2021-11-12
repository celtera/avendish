#pragma once
#include <utility>

/* SPDX-License-Identifier: GPL-3.0-or-later */

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

}
