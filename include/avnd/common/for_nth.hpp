#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/aggregates.hpp>
#include <avnd/common/coroutines.hpp>
#include <avnd/common/index.hpp>
#include <avnd/concepts/generic.hpp>

#include <cassert>
#include <utility>

namespace boost::pfr
{

template <class T, class F>
  requires(!avnd::vector_ish<std::decay_t<T>>)
constexpr void for_each_field_ref(T&& value, F&& func)
{
#if !defined(_MSC_VER)
  static_assert(!requires { value.size(); });
#endif
#if AVND_USE_BOOST_PFR
  using namespace pfr;
  using namespace pfr::detail;
  AVND_STATIC_CONSTEXPR std::size_t fields_count_val
      = boost::pfr::tuple_size_v<std::remove_reference_t<T>>;

  auto t = boost::pfr::detail::tie_as_tuple(
      value, boost::pfr::detail::size_t_<fields_count_val>{});
  [&]<std::size_t... I>(std::index_sequence<I...>)
  {
    (func(get<I>(t)), ...);
  }
  (std::make_index_sequence<fields_count_val>{});
#else
  auto&& [... elts] = value;
  (func(elts), ...);
#endif
}

template <typename T, class F>
  requires avnd::vector_ish<std::decay_t<T>>
void for_each_field_ref(T&& value, F&& func)
{
  for(auto& v : value)
  {
    func(v);
  }
}

}

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
  requires(!avnd::vector_ish<std::decay_t<T>>)
constexpr void for_each_field_ref(T&& value, F&& func)
{
#if !defined(_MSC_VER)
  static_assert(!requires { value.size(); });
#endif
#if AVND_USE_BOOST_PFR
  using namespace pfr;
  using namespace pfr::detail;
  AVND_STATIC_CONSTEXPR std::size_t fields_count_val
      = avnd::pfr::tuple_size_v<std::remove_reference_t<T>>;

  auto t = avnd::pfr::detail::tie_as_tuple(value);

  [&]<std::size_t... I>(std::index_sequence<I...>)
  {
    (func(get<I>(t)), ...);
  }
  (std::make_index_sequence<fields_count_val>{});
#else
  auto&& [... elts] = value;
  (func(elts), ...);
#endif
}

template <class T, class F>
  requires(avnd::vector_ish<std::decay_t<T>>)
constexpr void for_each_field_ref(T&& value, F&& func)
{
  for(auto& v : value) func(v);
}

template <class T, class F>
  requires(!avnd::vector_ish<T>)
constexpr void for_each_field_ref_n(T&& value, F&& func)
{
#if AVND_USE_BOOST_PFR
  using namespace pfr;
  using namespace pfr::detail;
  AVND_STATIC_CONSTEXPR std::size_t fields_count_val
      = avnd::pfr::tuple_size_v<std::remove_reference_t<T>>;
  auto t = avnd::pfr::detail::tie_as_tuple(value);

  [&]<std::size_t... I>(std::index_sequence<I...>)
  {
    (func(get<I>(t), avnd::field_index<I>{}), ...);
  }
  (std::make_index_sequence<fields_count_val>{});
#else
  auto&& [... elts] = value;
  const auto [... Is] = field_indices(std::make_index_sequence<sizeof...(elts)>{});

  (func(elts, Is), ...);
#endif
}

template <class T, class F>
void for_each_field_ref(avnd::member_iterator<T>&& value, F&& func)
{
  for(auto& v : value)
  {
    for_each_field_ref(v, func);
  }
}
template <class T, class F>
void for_each_field_ref(avnd::member_iterator<T>& value, F&& func)
{
  for(auto& v : value)
  {
    for_each_field_ref(v, func);
  }
}
template <class T, class F>
void for_each_field_ref(const avnd::member_iterator<T>& value, F&& func)
{
  for(auto& v : value)
  {
    for_each_field_ref(v, func);
  }
}

/* FIXME TBD
template <typename T, typename R>
constexpr void for_each_field_function_table(T&& value, R func)
{
#if !defined(_MSC_VER)
  static_assert(!requires { value.size(); });
#endif
  using namespace avnd::pfr;
  using namespace avnd::pfr::detail;
  AVND_STATIC_CONSTEXPR std::size_t fields_count_val
      = avnd::pfr::tuple_size_v<std::remove_reference_t<T>>;

  auto t = avnd::pfr::detail::tie_as_tuple(value);

  return [&]<std::size_t... I>(std::index_sequence<I...>) {
    return std::make_tuple(
        []<typename... Args>(Args... args) { R{}(get<I>(t), std::forward<Args>(args)...); }...);
  }(std::make_index_sequence<fields_count_val>{});
}
*/

// https://stackoverflow.com/questions/79757242/converting-pointer-to-member-to-member-index-using-boost-pfr-without-creating-an


#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-internal"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#endif

template <class T>
struct fake_object_wrapper { T const value; };

template <class T>
extern const fake_object_wrapper<T> fake_object_for_indices;

template<class T>
static constexpr auto fake_object_member_ptrs = std::apply([](auto const&... ref) {
  return std::array{static_cast<void const*>(std::addressof(ref))...};
}, boost::pfr::structure_tie(fake_object_for_indices<T>.value));

template<auto MemPtr>
static constexpr size_t index_of = []<class T, class V>(V T::* p) {
  return std::find(fake_object_member_ptrs<T>.begin(), fake_object_member_ptrs<T>.end(),
                   std::addressof(fake_object_for_indices<T>.value.*p)) - fake_object_member_ptrs<T>.begin();
}(MemPtr);

#ifdef __clang__
#pragma clang diagnostic pop
#endif
template <auto M>
static constexpr int index_in_struct_static()
{
  static constexpr auto idx = index_of<M>;
  return idx;
}

constexpr int index_in_struct(const auto& s, auto... member)
{
  static_assert(sizeof...(member) > 0);
  int index = -1;
  int k = 0;

  avnd::for_each_field_ref(s, [&, member...](auto& m) {
    if constexpr(requires { bool(&m == &(s.*....*member)); })
    {
      if(&m == &(s.*....*member))
      {
        index = k;
      }
    }
    ++k;
  });
#if !defined(_MSC_VER)
  assert(index >= 0);
#endif
  return index;
}
}
