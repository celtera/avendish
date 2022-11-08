#if __has_include(<tuplet/tuple.hpp>)
#include <tuplet/tuple.hpp>
namespace tpl = tuplet;
#else
namespace tpl = std;
#endif
namespace avnd
{
namespace pfr
{
constexpr auto structure_to_typelist(const auto& s) noexcept
{
  const auto& [... elts] = s;
  return typelist<std::decay_t<decltype(elts)>...>{};
}
constexpr auto structure_to_tuple(const auto& s) noexcept
{
  const auto& [... elts] = s;
  return tpl::make_tuple(elts...);
}

namespace detail
{
namespace sequence_tuple = ::tpl;
template <typename S>
constexpr auto tie_as_tuple(S&& s) noexcept
{
  auto&& [... elts] = static_cast<S&&>(s);
  return tpl::tie(elts...);
}
}

template <class T>
constexpr auto fields_count_impl(const T& t) noexcept
{
  const auto& [... elts] = t;
  return avnd::num<sizeof...(elts)>{};
}

template <class T>
constexpr std::size_t fields_count() noexcept
{
  return decltype(fields_count_impl(std::declval<const T&>()))::value;
}

template <typename T>
static constexpr const std::size_t tuple_size_v = fields_count<T>();

template <std::size_t I, typename S>
constexpr auto& get(S&& s) noexcept
{
  if constexpr(I == 0)
  {
    auto&& [a, ... elts] = static_cast<S&&>(s);
    return a;
  }
  else if constexpr(I == 1)
  {
    auto&& [a, b, ... elts] = static_cast<S&&>(s);
    return b;
  }
  else if constexpr(I == 2)
  {
    auto&& [a, b, c, ... elts] = static_cast<S&&>(s);
    return c;
  }
  else if constexpr(I == 3)
  {
    auto&& [a, b, c, d, ... elts] = static_cast<S&&>(s);
    return d;
  }
  else if constexpr(I == 4)
  {
    auto&& [a, b, c, d, e, ... elts] = static_cast<S&&>(s);
    return e;
  }
  else if constexpr(I == 5)
  {
    auto&& [a, b, c, d, e, f, ... elts] = static_cast<S&&>(s);
    return f;
  }
  else if constexpr(I == 6)
  {
    auto&& [a, b, c, d, e, f, g, ... elts] = static_cast<S&&>(s);
    return g;
  }
  else if constexpr(I == 7)
  {
    auto&& [a, b, c, d, e, f, g, h, ... elts] = static_cast<S&&>(s);
    return h;
  }
  else if constexpr(I == 8)
  {
    auto&& [a, b, c, d, e, f, g, h, i, ... elts] = static_cast<S&&>(s);
    return i;
  }
  else if constexpr(I == 9)
  {
    auto&& [a, b, c, d, e, f, g, h, i, j, ... elts] = static_cast<S&&>(s);
    return j;
  }
  else if constexpr(I == 10)
  {
    auto&& [a, b, c, d, e, f, g, h, i, j, k, ... elts] = static_cast<S&&>(s);
    return k;
  }
  else if constexpr(I == 11)
  {
    auto&& [a, b, c, d, e, f, g, h, i, j, k, l, ... elts] = static_cast<S&&>(s);
    return l;
  }
  else if constexpr(I == 12)
  {
    auto&& [a, b, c, d, e, f, g, h, i, j, k, l, m, ... elts] = static_cast<S&&>(s);
    return m;
  }
  else if constexpr(I > 12)
  {
    auto&& [... elts] = static_cast<S&&>(s);
    return tpl::get<I>(tpl::tie(elts...));
  }
}

template <std::size_t Index, typename T>
using tuple_element_t = std::decay_t<decltype(get<Index>(std::declval<const T&>()))>;

template <typename S>
constexpr auto for_each_field(S&& s, auto&& f) noexcept
{
  auto&& [... elts] = std::forward<S>(s);
  (f(elts), ...);
}
}

template <class T>
constexpr std::size_t fields_count_unsafe() noexcept
{
  return pfr::fields_count<T>();
}

template <typename T>
using as_tuple_ref = decltype(pfr::detail::tie_as_tuple(std::declval<T&>()));
template <typename T>
using as_tuple = decltype(pfr::structure_to_tuple(std::declval<T&>()));
template <typename T>
using as_typelist = decltype(pfr::structure_to_typelist(std::declval<T&>()));

// Yields a tuple with the compile-time function applied to each member of the struct
template <template <typename...> typename F, typename S>
constexpr auto struct_apply_impl(S&& s) noexcept
{
  auto&& [... elts] = std::forward<S>(s);
  return tpl::make_tuple(F<decltype(elts)>{}...);
}
template <template <typename...> typename F, typename T>
using struct_apply = decltype(struct_apply_impl(std::declval<T&&>()));
}
