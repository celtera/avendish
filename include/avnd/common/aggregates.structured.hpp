#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Aggregates backend for C++26-capable compilers (clang >= 21 / gcc >= 16).
// Used for BOTH the P1061 (no reflection) and the reflection configurations:
// the flattening needs only structured bindings, not std::meta.
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
// The flattening is built on STRUCTURED-BINDING PACKS (`auto&& [...elts] = v`),
// not std::meta member enumeration: std::meta member queries are consteval-only
// so they cannot be cached and get re-evaluated O(N^2) times (the dominant cost
// in -ftime-trace), whereas structured bindings are a cheap compiler builtin.
// This makes large objects ~10-20x faster to compile and, as a bonus, works on
// members of types nested in class templates — which std::meta cannot reflect on
// the clang-p2996 fork (llvm#138018). Static reflection is still used elsewhere
// (names, annotations); only the flattening avoids it.

#include <avnd/common/aggregates.base.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <tuple>
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

// A plain leaf port: neither a recursive group nor an expandable array.
template <typename T>
concept is_leaf = !is_recursive_group<std::remove_cvref_t<T>>
                  && !is_expandable_array<std::remove_cvref_t<T>>;

template <typename T>
constexpr auto flat_tie(T& v);

// Flatten one member reference: recurse into groups, expand arrays, else tie it.
template <typename E>
constexpr auto tie_one(E& e)
{
  using S = std::remove_cvref_t<E>;
  if constexpr(is_expandable_array<S>)
    return [&]<std::size_t... J>(std::index_sequence<J...>) {
      return tpl::tuple_cat(tie_one(e[J])...);
    }(std::make_index_sequence<std::extent_v<S>>{});
  else if constexpr(is_recursive_group<S>)
    return flat_tie(e);
  else
    return tpl::tie(e);
}

// The flattened tuple of leaf references, via a structured-binding pack.
template <typename T>
constexpr auto flat_tie(T& v)
{
  auto&& [... elts] = v;
  if constexpr((is_leaf<decltype(elts)> && ...))
    // All-leaf level: tie in ONE pack expansion. A wide N-way std::tuple_cat is
    // ~O(N^2) (catastrophic on libc++); a single std::tie is near-linear.
    return tpl::tie(elts...);
  else
    // Hierarchical level: cat the per-member sub-tuples. Each cat is only as wide
    // as one struct level, so tuple_cat stays cheap.
    return tpl::tuple_cat(tie_one(elts)...);
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
using flat_tuple_t
    = decltype(detail::flat_tie(std::declval<std::remove_cvref_t<T>&>()));

// tuple_size_v is used in CONSTRAINT EXPRESSIONS on arbitrary port value types
// (e.g. `requires(avnd::pfr::tuple_size_v<T> > 1)` where T may be a non-aggregate
// like std::unordered_map). It must therefore be SFINAE-friendly: instantiating
// flat_tuple_t on a non-aggregate hard-errors (structured-binding decomposition of
// a type with private members). Mirror the boost.pfr backend: the primary template
// is a `void*` (so the `> 1` comparison is a clean substitution failure), and only
// the aggregate-constrained specialization yields the real size_t.
template <typename T>
inline constexpr void* tuple_size_v{};

template <typename T>
  requires(aggregate_argument<std::remove_cvref_t<T>>)
inline constexpr std::size_t tuple_size_v<T> = std::tuple_size_v<flat_tuple_t<T>>;

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
