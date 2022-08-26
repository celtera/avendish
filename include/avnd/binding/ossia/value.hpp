#pragma once
#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <ossia/network/value/value.hpp>

namespace oscr
{

template <typename T>
concept type_wrapper = requires(T t)
{
  sizeof(typename T::wrapped_type);
};

template <typename T>
consteval bool vecf_compatible()
{
#define is_fp(...) std::is_floating_point_v<std::decay_t<__VA_ARGS__>>
  constexpr int sz = avnd::pfr::tuple_size_v<T>;
  if constexpr(sz == 2)
  {
    return is_fp(avnd::pfr::tuple_element_t<0, T>) && is_fp(avnd::pfr::tuple_element_t<1, T>);
  }
  else if constexpr(sz == 3)
  {
      return is_fp(avnd::pfr::tuple_element_t<0, T>) && is_fp(avnd::pfr::tuple_element_t<1, T>)  && is_fp(avnd::pfr::tuple_element_t<2, T>);
  }
  else if constexpr(sz == 4)
  {
      return is_fp(avnd::pfr::tuple_element_t<0, T>) && is_fp(avnd::pfr::tuple_element_t<1, T>)  && is_fp(avnd::pfr::tuple_element_t<2, T>) && is_fp(avnd::pfr::tuple_element_t<3, T>);
  }
#undef is_fp
  return false;
}

}
