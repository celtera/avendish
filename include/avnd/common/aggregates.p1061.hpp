#pragma once
#include <avnd/common/aggregates.base.hpp>
#include <array>

#if defined(__has_attribute)
#if __has_attribute(__builtin_structured_binding_size)
#define AVND_HAS_STRUCTURED_BINDING_SIZE_ATTRIBUTE 1
#endif
#endif

namespace avnd
{
namespace pfr
{
AVND_INLINE
constexpr auto structure_to_typelist(const auto& s) noexcept
{
  const auto& [... elts] = s;
  return typelist<std::decay_t<decltype(elts)>...>{};
}
AVND_INLINE
constexpr auto structure_to_tuple(const auto& s) noexcept
{
  const auto& [... elts] = s;
  return tpl::make_tuple(elts...);
}

namespace detail
{
namespace sequence_tuple = ::tpl;
template <typename S>
AVND_INLINE constexpr auto tie_as_tuple(S&& s) noexcept
{
  auto&& [... elts] = static_cast<S&&>(s);
  return tpl::tie(std::forward_like<S>(elts)...);
}
}

#if !defined(AVND_HAS_STRUCTURED_BINDING_SIZE_ATTRIBUTE)
template <class T>
AVND_INLINE constexpr auto fields_count_impl(const T& t) noexcept
{
  const auto& [... elts] = t;
  return avnd::num<sizeof...(elts)>{};
}

template <typename T>
static constexpr const std::size_t tuple_size_v = decltype(fields_count_impl<T>(std::declval<const T&>()))::value;
# else

template <typename T>
static constexpr const std::size_t tuple_size_v = __builtin_structured_binding_size(T);
#endif

template <std::size_t I, typename S>
AVND_INLINE constexpr auto&& get(S&& s) noexcept
{
  auto&& [... elts] = static_cast<S&&>(s);
  return std::forward_like<S>(elts...[I]);
}

template <std::size_t Index, typename T>
using tuple_element_t = std::decay_t<decltype(get<Index>(std::declval<const T&>()))>;

template <typename S, typename F>
AVND_INLINE constexpr auto for_each_field(S&& s, F&& f) noexcept
{
  auto&& [... elts] = static_cast<S&&>(s);
  ((static_cast<F&&>(f)(std::forward_like<S>(elts))), ...);
}
}

template <typename T>
using as_typelist = decltype(pfr::structure_to_typelist(std::declval<T&>()));


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
static constexpr auto fake_object_member_ptrs = []() {
  auto&& [... ref] = fake_object_for_indices<T>.value;
  return std::array{static_cast<void const*>(std::addressof(ref))...};
}();

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


}
