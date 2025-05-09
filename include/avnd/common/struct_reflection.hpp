#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/aggregates.hpp>
#include <avnd/common/coroutines.hpp>
#include <avnd/common/dummy.hpp>
#include <avnd/common/errors.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/common/index.hpp>
#include <avnd/common/index_sequence.hpp>
#include <boost/mp11.hpp>

namespace avnd
{
// Select a subset of fields and apply an operation on them

template <
    template <typename...> typename F, template <typename...> typename Filter,
    typename T>
using filter_and_apply = typename Filter<T>::template filter_and_apply<F>;

/**
 * Utilities to introspect all fields in a struct
 */
template <typename T>
struct fields_introspection
{
  using type = T;

  static constexpr auto size = pfr::tuple_size_v<type>;
  using indices_n = std::make_integer_sequence<int, size>;

  template <std::size_t N>
  using field_type = pfr::tuple_element_t<N, T>;

  static constexpr void for_all(auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      // C++20 : template lambda
      [&func]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        (func(field_reflection<Index, pfr::tuple_element_t<Index, type>>{}), ...);
          }(indices_n{});
    }
  }

  static constexpr void for_nth(int n, auto&& func) noexcept
  {
    // TODO maybe there is some dirty hack to do here with offsetof computations...
    // technically, we ought to be able to compute the address of member "n" at compile time...
    // and reinterpret_cast &fields+addr....

    if constexpr(size > 0)
    {
      [n, &func]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        // TODO compare with || logical-or fold ?
        (void)((Index == n && (func(field_reflection<Index, pfr::tuple_element_t<Index, type>>{}), true)) || ...);
          }(indices_n{});
    }
  }

  template <typename F>
  static constexpr void for_all(type& fields, F&& func) noexcept
  {
#if AVND_USE_BOOST_PFR
    if constexpr(size > 0)
    {
      avnd::for_each_field_ref(fields, std::forward<F>(func));
    }
#else
    auto&& [... elts] = fields;
    (func(elts), ...);
#endif
  }

  template <typename F>
  static constexpr void for_all_n(type& fields, F&& func) noexcept
  {
#if AVND_USE_BOOST_PFR
    if constexpr(size > 0)
    {
      avnd::for_each_field_ref_n(fields, std::forward<F>(func));
    }
#else
    // FIXME pass it the avnd::field_index<I>
    auto&& [... elts] = fields;
    const auto [... Is] = field_indices(std::make_index_sequence<sizeof...(elts)>{});

    (func(elts, Is), ...);
#endif
  }

  static constexpr void for_nth(type& fields, int n, auto&& func) noexcept
  {
    // TODO maybe there is some dirty hack to do here with offsetof computations...
    // technically, we ought to be able to compute the address of member "n" at compile time...
    // and reinterpret_cast &fields+addr....

    if constexpr(size > 0)
    {
#if AVND_USE_BOOST_PFR
      using namespace std;
      [n, &func, &fields]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        auto&& ppl = pfr::detail::tie_as_tuple(fields);
        // TODO compare with || logical-or fold ?
        (void)((Index == n && (func(get<Index>(ppl)), true)) || ...);
          }(indices_n{});
#else
      [n, &func, &fields]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        auto&& [... elts] = fields;
        // TODO compare with || logical-or fold ?
        (void)((Index == n && (func(elts), true)) || ...);
          }(indices_n{});
#endif
    }
  }

  template <std::size_t N>
  static inline constexpr auto field(type& unfiltered_fields) noexcept -> decltype(auto)
  {
#if AVND_USE_BOOST_PFR
    return pfr::get<N>(unfiltered_fields);
#else
    auto&& [... elts] = unfiltered_fields;
    return std::forward_like<type&>(elts...[N]);
#endif
  }

  // Will stop if an error is encountered (func should return bool)
  static constexpr bool for_all_unless(type& unfiltered_fields, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
#if AVND_USE_BOOST_PFR
      auto stack_size_helper = [&]<std::size_t Index>() constexpr noexcept {
        return func(pfr::get<Index>(unfiltered_fields));
      };
      return [stack_size_helper]<typename K, K... Index>(
                 std::integer_sequence<K, Index...>) {
        return (stack_size_helper.template operator()<Index>() && ...);
      }(indices_n{});
#else
      auto&& [... elts] = unfiltered_fields;
      return (func(elts) && ...);
#endif
    }
    else
    {
      return true;
    }
  }

  // Will stop if a call to func() returns true
  static constexpr bool for_all_until(type& unfiltered_fields, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
#if AVND_USE_BOOST_PFR
      auto stack_size_helper = [&]<std::size_t Index>() constexpr noexcept {
        return func(pfr::get<Index>(unfiltered_fields));
      };
      return [stack_size_helper]<typename K, K... Index>(
                 std::integer_sequence<K, Index...>) {
        return (stack_size_helper.template operator()<Index>() || ...);
      }(indices_n{});
#else
      auto&& [... elts] = unfiltered_fields;
      return (func(elts) || ...);
#endif
    }
    else
    {
      return false;
    }
  }
};

/**
 * Utilities to introspect all fields in a struct which match a given predicate
 */

template <typename T, template <typename...> typename P, typename IntT>
struct matches_predicate : P<pfr::tuple_element_t<IntT::value, T>>
{
};

template <typename T, template <typename...> typename P>
struct predicate_introspection
{
  using type = T;
  using indices
      = typed_index_sequence_t<std::make_index_sequence<pfr::tuple_size_v<type>>>;

  template <typename IntT>
  struct matches_predicate_i : matches_predicate<T, P, IntT>
  {
  };

  using indices_n
      = numbered_index_sequence_t<boost::mp11::mp_copy_if<indices, matches_predicate_i>>;

  using fields = boost::mp11::mp_copy_if<as_typelist<type>, P>;

  template <template <typename...> typename F>
  using filter_and_apply
      = boost::mp11::mp_rename<boost::mp11::mp_transform<F, fields>, tpl::tuple>;

  static constexpr auto index_map = integer_sequence_to_array(indices_n{});
  static constexpr auto size = indices_n::size();

  // TODO consteval when clang < 14 is dropped
  template <std::size_t Idx>
  static constexpr int map() noexcept
  {
    return index_map[Idx];
  }
  template <std::size_t Idx>
  static constexpr int unmap() noexcept
  {
    return avnd::index_of_element<Idx>(index_map);
  }

  template <std::size_t Idx>
  static constexpr auto index_to_field_index(avnd::predicate_index<Idx>) noexcept
  {
    return avnd::field_index<index_map[Idx]>{};
  }
  template <std::size_t Idx>
  static constexpr auto field_index_to_index(avnd::field_index<Idx>) noexcept
  {
    return avnd::predicate_index<unmap<Idx>()>{};
  }

  static constexpr auto index_to_field_index(int pred_idx) noexcept
  {
    return index_map[pred_idx];
  }
  static constexpr auto field_index_to_index(int field_idx) noexcept
  {
    return avnd::index_of_element(field_idx, indices_n{});
  }

  static constexpr auto field_reflections() noexcept
  {
    return []<typename K, K... Index>(std::integer_sequence<K, Index...>) {
      return avnd::typelist<
          field_reflection<Index, pfr::tuple_element_t<Index, T>>...>{};
    }(indices_n{});
  }
  using field_reflections_type = std::decay_t<decltype(field_reflections())>;

  static constexpr void for_all(auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      [&func]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        (func(field_reflection<Index, pfr::tuple_element_t<Index, T>>{}), ...);
          }(indices_n{});
    }
  }

  // n is in [0; total number of ports[ (even those that don't match the predicate)
  static constexpr void for_nth_raw(int n, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      [n, &func]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        // TODO compare with || logical-or fold ?
        (void)((Index == n && (func(field_reflection<Index, pfr::tuple_element_t<Index, T>>{}), true)) || ...);
          }(indices_n{});
    }
  }

  // n is in [0; number of ports matching that predicate[
  static constexpr void for_nth_mapped(int n, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      [k = index_map[n],
       &func]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        // TODO compare with || logical-or fold ?
        (void)((Index == k && (func(field_reflection<Index, pfr::tuple_element_t<Index, T>>{}), true)) || ...);
          }(indices_n{});
    }
  }

  // Goes from 0, 1, 2 indices to indices in the complete
  // struct with members that may not match this
#if defined(_MSC_VER)
  template <std::size_t N>
  static constexpr int nth_element_i = index_map[N];
  template <std::size_t N>
  using nth_element = avnd::pfr::tuple_element_t<nth_element_i<N>, type>;
#else
  template <std::size_t N>
  using nth_element = avnd::pfr::tuple_element_t<index_map[N], type>;
#endif

  template <std::size_t N>
  static constexpr auto field(type& unfiltered_fields) noexcept -> decltype(auto)
  {
#if AVND_USE_BOOST_PFR
    return pfr::get<index_map[N]>(unfiltered_fields);
#else
    auto& [... elts] = unfiltered_fields;
    return std::forward_like<type&>(elts...[index_map[N]]);
#endif
  }

  // Gives std::tuple<field1&, field2&, etc...>
  static constexpr auto tie(type& unfiltered_fields)
  {
    return [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
#if AVND_USE_BOOST_PFR
      return tpl::tie(pfr::get<Index>(unfiltered_fields)...);
#else
      auto&& [... elts] = unfiltered_fields;
      return tpl::tie(elts...[Index]...);
#endif
    }(indices_n{});
  }

  // Gives std::tuple<field1, field2, etc...>
  static constexpr auto make_tuple(type& unfiltered_fields)
  {
    return [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
#if AVND_USE_BOOST_PFR
      return tpl::make_tuple(pfr::get<Index>(unfiltered_fields)...);
#else
      auto&& [... elts] = unfiltered_fields;
      return tpl::make_tuple(elts...[Index]...);
#endif
    }(indices_n{});
  }

  // Gives std::tuple<f(field1), f(field2), etc...>
  static constexpr auto filter_tuple(type& unfiltered_fields, auto filter)
  {
#if AVND_USE_BOOST_PFR
    auto stack_size_helper = [&]<std::size_t Index>() constexpr noexcept {
      return filter(pfr::get<Index>(unfiltered_fields));
    };
    return [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
      return tpl::make_tuple(stack_size_helper.template operator()<Index>()...);
    }(indices_n{});
#else
    return [&]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
      auto&& [... elts] = unfiltered_fields;
      return tpl::make_tuple(filter(elts...[Index])...);
    }(indices_n{});
#endif
  }

  static constexpr void for_all(type& unfiltered_fields, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      [&func,
       &unfiltered_fields]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
#if AVND_USE_BOOST_PFR
        (func(pfr::get<Index>(unfiltered_fields)), ...);
#else
        auto&& [... elts] = unfiltered_fields;
        (func(elts...[Index]), ...);
#endif
      }(indices_n{});
    }
  }

  template <typename U>
  static constexpr void
  for_all(member_iterator<U>&& unfiltered_fields, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      [&func,
       &unfiltered_fields]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        for(auto& m : unfiltered_fields)
        {
          (func(pfr::get<Index>(m)), ...);
        }
          }(indices_n{});
    }
  }

  template <typename U>
  static constexpr void
  for_all(member_iterator<U>& unfiltered_fields, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      [&func,
       &unfiltered_fields]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
        for(auto& m : unfiltered_fields)
        {
          (func(pfr::get<Index>(m)), ...);
        }
          }(indices_n{});
    }
  }

  // Same as for_all but also passes the predicate-based index (0, 1, 2) as template argument
  static constexpr void for_all_n(type& unfiltered_fields, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      auto stack_size_helper
          = [&]<std::size_t Index, std::size_t LocalIndex>() constexpr noexcept {
        func(pfr::get<Index>(unfiltered_fields), avnd::predicate_index<LocalIndex>{});
      };
      [stack_size_helper]<typename K, K... Index, size_t... LocalIndex>(
          std::integer_sequence<K, Index...>,
          std::integer_sequence<size_t, LocalIndex...>) {
        (stack_size_helper.template operator()<Index, LocalIndex>(), ...);
          }(indices_n{}, std::make_index_sequence<size>{});
    }
  }

  // Same as for_all_n but also passes the struct-based index (2, 7, 12) as template argument
  static constexpr void for_all_n2(type& unfiltered_fields, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      auto stack_size_helper
          = [&]<std::size_t Index, std::size_t LocalIndex>() constexpr noexcept {
        func(
            pfr::get<Index>(unfiltered_fields), avnd::predicate_index<LocalIndex>{},
            avnd::field_index<Index>{});
      };

      [stack_size_helper]<typename K, K... Index, size_t... LocalIndex>(
          std::integer_sequence<K, Index...>,
          std::integer_sequence<size_t, LocalIndex...>) {
        (stack_size_helper.template operator()<Index, LocalIndex>(), ...);
          }(indices_n{}, std::make_index_sequence<size>{});
    }
  }

  template <typename U>
  static constexpr void
  for_all_n(member_iterator<U> unfiltered_fields, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      auto stack_size_helper
          = [&]<std::size_t Index, std::size_t LocalIndex>(auto& m) constexpr noexcept {
        func(pfr::get<Index>(m), avnd::predicate_index<LocalIndex>{});
      };
      [stack_size_helper,
       &unfiltered_fields]<typename K, K... Index, size_t... LocalIndex>(
          std::integer_sequence<K, Index...>,
          std::integer_sequence<size_t, LocalIndex...>) {
        for(auto& m : unfiltered_fields)
        {
          (stack_size_helper.template operator()<Index, LocalIndex>(m), ...);
        }
          }(indices_n{}, std::make_index_sequence<size>{});
    }
  }

  // Will stop if an error is encountered (func should return bool)
  static constexpr bool for_all_unless(type& unfiltered_fields, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      auto stack_size_helper = [&]<std::size_t Index>() constexpr noexcept {
        return func(pfr::get<Index>(unfiltered_fields));
      };
      return [stack_size_helper]<typename K, K... Index>(
                 std::integer_sequence<K, Index...>) {
        return (stack_size_helper.template operator()<Index>() && ...);
      }(indices_n{});
    }
    else
    {
      return true;
    }
  }

  // Will stop if a call to func() returns true
  static constexpr bool for_all_until(type& unfiltered_fields, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      auto stack_size_helper = [&]<std::size_t Index>() constexpr noexcept {
        return func(pfr::get<Index>(unfiltered_fields));
      };
      return [stack_size_helper]<typename K, K... Index>(
                 std::integer_sequence<K, Index...>) {
        return (stack_size_helper.template operator()<Index>() || ...);
      }(indices_n{});
    }
    else
    {
      return false;
    }
  }

  template <typename U>
  static constexpr bool
  for_all_unless(member_iterator<U> unfiltered_fields, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      AVND_ERROR(U, "Cannot use for_all_unless when there are multiple instances");
      return false;
    }
    else
    {
      return true;
    }
  }

  template <typename U>
  static constexpr bool
  for_all_until(member_iterator<U> unfiltered_fields, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      AVND_ERROR(U, "Cannot use for_all_until when there are multiple instances");
      return false;
    }
    else
    {
      return true;
    }
  }

  static constexpr void for_nth_raw(type& fields, int n, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      [n, &func, &fields]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
#if AVND_USE_BOOST_PFR
        (void)((Index == n && (func(pfr::get<Index>(fields)), true)) || ...);
#else
        auto&& [... elts] = fields;
        (void)((Index == n && (func(elts...[Index]), true)) || ...);
#endif
          }(indices_n{});
    }
  }

  static constexpr void for_nth_mapped(type& fields, int n, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      [k = index_map[n], &func,
       &fields]<typename K, K... Index>(std::integer_sequence<K, Index...>) {
#if AVND_USE_BOOST_PFR
        (void)((Index == k && (func(pfr::get<Index>(fields)), true)) || ...);
#else
        auto&& [... elts] = fields;
        (void)((Index == k && (func(elts...[Index]), true)) || ...);
#endif
          }(indices_n{});
    }
  }
  static constexpr void
  for_nth_mapped_n2(type& unfiltered_fields, int n, auto&& func) noexcept
  {
    if constexpr(size > 0)
    {
      auto stack_size_helper
          = [&]<std::size_t Index, std::size_t LocalIndex>() constexpr noexcept {
        func(
            pfr::get<Index>(unfiltered_fields), avnd::predicate_index<LocalIndex>{},
            avnd::field_index<Index>{});
      };

      [stack_size_helper, k = index_map[n], &func,
       &unfiltered_fields]<typename K, K... Index, size_t... LocalIndex>(
          std::integer_sequence<K, Index...>,
          std::integer_sequence<size_t, LocalIndex...>) {
        (void)((Index == k
                && (stack_size_helper.template operator()<Index, LocalIndex>(), true))
               || ...);
      }(indices_n{}, std::make_index_sequence<size>{});
    }
  }
};

template <>
struct fields_introspection<avnd::dummy>
{
  using type = avnd::dummy;
  using fields = avnd::typelist<>;
  using field_reflections_type = avnd::typelist<>;
  static constexpr auto index_map = std::array<int, 0>{};
  static constexpr auto size = 0;

  static constexpr std::tuple<> tie(auto&& unfiltered_fields) { return {}; }
  static constexpr std::tuple<> make_tuple(auto&& unfiltered_fields) { return {}; }

  static constexpr void for_all(auto&& func) noexcept { }
  static constexpr void for_all_n(auto&& func) noexcept { }
  static constexpr void for_nth(int n, auto&& func) noexcept { }
  static constexpr void for_all_unless(auto&& func) noexcept { }
  static constexpr void for_all_until(auto&& func) noexcept { }
  static constexpr void for_all(avnd::dummy fields, auto&& func) noexcept { }
  static constexpr void for_nth(avnd::dummy fields, int n, auto&& func) noexcept { }
  static constexpr void for_all_unless(avnd::dummy fields, auto&& func) noexcept { }
  static constexpr void for_all_until(avnd::dummy fields, auto&& func) noexcept { }
};

template <template <typename...> typename P>
struct predicate_introspection<avnd::dummy, P>
{
  using type = avnd::dummy;
  using fields = avnd::typelist<>;
  using field_reflections_type = avnd::typelist<>;
  static constexpr auto index_map = std::array<int, 0>{};
  static constexpr auto size = 0;

  template <template <typename...> typename F>
  using filter_and_apply = std::tuple<>;

  template <std::size_t N>
  using nth_element = void;

  template <std::size_t N>
  static constexpr void field(avnd::dummy unfiltered_fields) noexcept
  {
  }

  static constexpr std::tuple<> tie(auto&& unfiltered_fields) { return {}; }
  static constexpr std::tuple<> make_tuple(auto&& unfiltered_fields) { return {}; }

  static constexpr void for_all(auto&& func) noexcept { }
  static constexpr void for_all_n(auto&& func) noexcept { }
  static constexpr void for_nth_raw(int n, auto&& func) noexcept { }
  static constexpr void for_nth_mapped(int n, auto&& func) noexcept { }
  static constexpr void for_all_unless(auto&& func) noexcept { }
  static constexpr void for_all(avnd::dummy fields, auto&& func) noexcept { }
  static constexpr void for_nth_raw(avnd::dummy fields, int n, auto&& func) noexcept { }
  static constexpr void for_nth_mapped(avnd::dummy fields, int n, auto&& func) noexcept
  {
  }
  static constexpr void for_all_unless(avnd::dummy fields, auto&& func) noexcept { }
};

}
