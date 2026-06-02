#pragma once
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/mp11/algorithm.hpp>
#include <boost/pfr.hpp>
#include <halp/polyfill.hpp>

#include <avnd/common/aggregates.recursive.hpp>
#include <avnd/common/aggregates.simple.hpp>

#include <array>

namespace avnd::pfr
{
template <typename T>
concept aggregate_argument = std::is_aggregate_v<std::decay_t<T>>;
namespace detail
{
template <class T>
  requires(aggregate_argument<T> && avnd::has_recursive_groups<T>)
constexpr AVND_INLINE auto tie_as_tuple(T& val)
{
  return pfr_recursive::detail::tie_as_tuple(val);
}
template <class T>
  requires(aggregate_argument<T> && !avnd::has_recursive_groups<T>)
constexpr AVND_INLINE auto tie_as_tuple(T& val)
{
  return pfr_simple::detail::tie_as_tuple(val);
}

}

template <typename T>
struct pfr_impl_helper;
template <typename T>
  requires(aggregate_argument<T> && avnd::has_recursive_groups<T>)
struct pfr_impl_helper<T>
{
  using as_typelist = pfr_recursive::as_typelist<T>;
  using tuple_size = pfr_recursive::tuple_size<T>;
  template <std::size_t N>
  using tuple_element_t = pfr_recursive::tuple_element_t<N, T>;
};
template <typename T>
  requires(aggregate_argument<T> && !avnd::has_recursive_groups<T>)
struct pfr_impl_helper<T>
{
  using as_typelist = pfr_simple::as_typelist<T>;
  using tuple_size = pfr_simple::tuple_size<T>;
  template <std::size_t N>
  using tuple_element_t = pfr_simple::tuple_element_t<N, T>;
};

// template <typename T>
// using tuple_size = typename pfr_impl_helper<T>::tuple_size;
template <typename T>
inline constexpr void* tuple_size_v{};

template <typename T>
  requires(aggregate_argument<std::decay_t<T>> && avnd::has_recursive_groups<T>)
inline constexpr size_t tuple_size_v<T> = pfr_recursive::tuple_size<T>::value;

template <typename T>
  requires(aggregate_argument<T> && !avnd::has_recursive_groups<T>)
inline constexpr size_t tuple_size_v<T> = pfr_simple::tuple_size<T>::value;

template <std::size_t N, typename T>
  requires(aggregate_argument<T> && avnd::has_recursive_groups<T>)
constexpr AVND_INLINE auto& get(T&& v)
{
  return pfr_recursive::get<N>(std::forward<T>(v));
}

template <std::size_t N, typename T>
  requires(aggregate_argument<T> && !avnd::has_recursive_groups<T>)
constexpr AVND_INLINE auto& get(T&& v)
{
  return pfr_simple::get<N>(std::forward<T>(v));
}

template <std::size_t N, typename T>
using tuple_element_t = typename pfr_impl_helper<T>::template tuple_element_t<N>;

template <typename... Args>
AVND_INLINE constexpr auto for_each_field(Args&&... args) noexcept(
    noexcept(boost::pfr::for_each_field(std::forward<Args>(args)...))) -> decltype(auto)
{
  return boost::pfr::for_each_field(std::forward<Args>(args)...);
}
}

namespace avnd
{

template <typename T>
using as_typelist = typename pfr::pfr_impl_helper<T>::as_typelist;


#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-internal"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#endif

template <class T>
struct fake_object_wrapper { T const value; };

#if defined(_MSC_VER) && !defined(__clang__)
// MSVC materializes fake_object_member_ptrs<T> as a static array of pointers into
// fake_object_for_indices<T> and emits an external reference to that symbol; with an
// `extern` (undefined) declaration this then fails to link (LNK2001). Give it an actual
// definition instead. The object is never read, only its members' addresses are taken to
// compute member indices at compile time, so value-initialization is enough.
template <class T>
inline const fake_object_wrapper<T> fake_object_for_indices{};
#else
// Clang/GCC fold the address arithmetic down to integer indices at compile time and never
// ODR-use the variable, so it is intentionally left undefined (cf. the
// -Wundefined-var-template suppression above) to avoid having to construct a T.
template <class T>
extern const fake_object_wrapper<T> fake_object_for_indices;
#endif

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
  static_constexpr auto idx = index_of<M>;
  return idx;
}
}
