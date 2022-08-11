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
#define is_fp(X) std::is_floating_point_v<std::decay_t<decltype(X)>>
  constexpr int sz = avnd::pfr::tuple_size_v<T>;
  if constexpr(sz == 2)
  {
    auto [x, y] = T{};
    return is_fp(x) && is_fp(y);
  }
  else if constexpr(sz == 3)
  {
    auto [x, y, z] = T{};
    return is_fp(x) && is_fp(y) && is_fp(z);
  }
  else if constexpr(sz == 4)
  {
    auto [x, y, z, w] = T{};
    return is_fp(x) && is_fp(y) && is_fp(z) && is_fp(w);
  }
#undef is_fp
  return false;
}

}
