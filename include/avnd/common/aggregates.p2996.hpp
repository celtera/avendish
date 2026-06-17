#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// C++26 static-reflection aggregates backend.
//
// Provides the avnd::pfr::* port-introspection interface (tie_as_tuple / get /
// tuple_size_v / tuple_element_t / as_typelist) with a FLATTENED view that
// natively handles:
//   - recursive_group members (structs flagged with `recursive_group`): their
//     leaves are inlined into the parent, replacing the boost.pfr recursive
//     flattening (aggregates.recursive.hpp);
//   - raw compile-time arrays of ports (E[N] of class type): expanded to N
//     leaves, so e.g. `DrumChannel voices[8];` works without manual unrolling.
//
// A flat struct with neither behaves exactly like the structured-binding backend.

#include <avnd/common/aggregates.base.hpp>
#include <avnd/common/meta_polyfill.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace avnd::pfr
{
template <typename T>
concept aggregate_argument = std::is_aggregate_v<std::decay_t<T>>;

namespace detail
{
// A struct opts into flattening by declaring `recursive_group` (cf. halp_flag).
template <typename T>
concept is_recursive_group = requires { std::remove_cvref_t<T>::recursive_group; };

// A raw array of class-typed ports expands into its elements.
template <typename S>
concept is_expandable_array
    = std::is_array_v<S> && std::is_class_v<std::remove_extent_t<S>>;

template <typename T>
consteval std::size_t direct_member_count()
{
  return std::meta::nonstatic_data_members_of(
             ^^T, std::meta::access_context::unchecked())
      .size();
}

template <typename T>
consteval std::array<std::meta::info, direct_member_count<T>()> direct_members()
{
  std::array<std::meta::info, direct_member_count<T>()> a{};
  std::size_t i = 0;
  for(std::meta::info m :
      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
    a[i++] = m;
  return a;
}

// Number of leaf ports once groups and port-arrays are expanded.
template <typename T>
consteval std::size_t leaf_count()
{
  std::size_t n = 0;
  template for(constexpr std::meta::info m : std::define_static_array(
                   std::meta::nonstatic_data_members_of(
                       ^^T, std::meta::access_context::unchecked())))
  {
    using S = std::remove_cvref_t<typename[:std::meta::type_of(m):]>;
    if constexpr(is_expandable_array<S>)
    {
      using E = std::remove_extent_t<S>;
      if constexpr(is_recursive_group<E>)
        n += std::extent_v<S> * leaf_count<E>();
      else
        n += std::extent_v<S>;
    }
    else if constexpr(is_recursive_group<S>)
      n += leaf_count<S>();
    else
      n += 1;
  }
  return n;
}

// --- runtime reference tying (members flow as template args: info is consteval-only) ---
template <typename T>
constexpr auto flat_tie(T& v);

template <typename E>
constexpr auto tie_elem(E& e)
{
  if constexpr(is_recursive_group<E>)
    return flat_tie(e);
  else
    return tpl::tie(e);
}

template <std::meta::info M, typename T>
constexpr auto tie_member(T& v)
{
  auto& sub = v.[:M:];
  using S = std::remove_reference_t<decltype(sub)>;
  if constexpr(is_expandable_array<S>)
  {
    return [&]<std::size_t... J>(std::index_sequence<J...>) {
      return tpl::tuple_cat(tie_elem(sub[J])...);
    }(std::make_index_sequence<std::extent_v<S>>{});
  }
  else if constexpr(is_recursive_group<S>)
    return flat_tie(sub);
  else
    return tpl::tie(sub);
}

template <std::meta::info... Ms, typename T>
constexpr auto flat_tie_impl(T& v)
{
  return tpl::tuple_cat(tie_member<Ms>(v)...);
}

template <typename T, std::size_t... I>
constexpr auto flat_tie_idx(T& v, std::index_sequence<I...>)
{
  return flat_tie_impl<direct_members<std::remove_cvref_t<T>>()[I]...>(v);
}

template <typename T>
constexpr auto flat_tie(T& v)
{
  return flat_tie_idx(
      v, std::make_index_sequence<direct_member_count<std::remove_cvref_t<T>>()>{});
}

// avnd::pfr::detail::tie_as_tuple — the flattened tuple of leaf references.
template <typename T>
AVND_INLINE constexpr auto tie_as_tuple(T& val)
{
  return flat_tie(val);
}

template <typename Tup>
struct flat_typelist;
template <typename... Ts>
struct flat_typelist<tpl::tuple<Ts...>>
{
  using type = avnd::typelist<std::remove_cvref_t<Ts>...>;
};
}

// --- public interface ---
template <typename T>
inline constexpr std::size_t tuple_size_v
    = detail::leaf_count<std::remove_cvref_t<T>>();

// Constrained to aggregates so that get<N>(some_std_tuple) falls back to the
// tuple's own get (matching the boost.pfr backend) instead of re-flattening it.
template <std::size_t N, typename T>
  requires(aggregate_argument<std::remove_cvref_t<T>>)
AVND_INLINE constexpr auto& get(T&& v)
{
  return tpl::get<N>(detail::flat_tie(v));
}

template <std::size_t N, typename T>
using tuple_element_t
    = std::remove_cvref_t<decltype(get<N>(std::declval<std::remove_cvref_t<T>&>()))>;

// Direct (non-flattened) decomposition, used on value types.
AVND_INLINE constexpr auto structure_to_typelist(const auto& s) noexcept
{
  const auto& [... elts] = s;
  return typelist<std::decay_t<decltype(elts)>...>{};
}
AVND_INLINE constexpr auto structure_to_tuple(const auto& s) noexcept
{
  const auto& [... elts] = s;
  return tpl::make_tuple(elts...);
}

template <typename S, typename F>
AVND_INLINE constexpr auto for_each_field(S&& s, F&& f) noexcept
{
  auto&& [... elts] = static_cast<S&&>(s);
  ((static_cast<F&&>(f)(std::forward_like<S>(elts))), ...);
}
}

namespace avnd
{
// Flattened leaf typelist.
template <typename T>
using as_typelist = typename pfr::detail::flat_typelist<decltype(pfr::detail::flat_tie(
    std::declval<std::remove_cvref_t<T>&>()))>::type;

// --- member-index machinery (raw struct order; same approach as the p1061 backend) ---
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-internal"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#endif

template <class T>
struct fake_object_wrapper
{
  T const value;
};

template <class T>
extern const fake_object_wrapper<T> fake_object_for_indices;

template <class T>
static constexpr auto fake_object_member_ptrs = []() {
  auto&& [... ref] = fake_object_for_indices<T>.value;
  return std::array{static_cast<void const*>(std::addressof(ref))...};
}();

template <auto MemPtr>
static constexpr size_t index_of = []<class T, class V>(V T::* p) {
  return std::find(
             fake_object_member_ptrs<T>.begin(), fake_object_member_ptrs<T>.end(),
             std::addressof(fake_object_for_indices<T>.value.*p))
         - fake_object_member_ptrs<T>.begin();
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
}
