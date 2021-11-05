#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <boost/mp11.hpp>
#include <boost/pfr.hpp>
#include <common/index_sequence.hpp>
#include <avnd/concepts/generic.hpp>

namespace avnd
{

template <typename T>
using as_tuple_ref
    = decltype(::boost::pfr::detail::tie_as_tuple(std::declval<T&>()));
template <typename T>
using as_tuple
    = decltype(::boost::pfr::structure_to_tuple(std::declval<T&>()));

// Yields a tuple with the compile-time function applied to each member of the struct
template <template <typename...> typename F, typename T>
using struct_apply = boost::mp11::mp_transform<F, as_tuple<T>>;

// Select a subset of fields and apply an operation on them
template <
    template <typename...>
    typename F,
    template <typename...>
    typename Filter,
    typename T>
using filter_and_apply
    = boost::mp11::mp_transform<F, typename Filter<T>::fields>;

template <std::size_t Idx, typename Field>
struct field_reflection
{
  using type = Field;
  static const constexpr auto index = Idx;
};

/**
 * Utilities to introspect all fields in a struct
 */
template <typename T>
struct fields_introspection
{
  using type = T;

  using fields = as_tuple<type>;
  using indices = typed_index_sequence_t<
      std::make_index_sequence<boost::pfr::tuple_size_v<type>>>;
  using indices_n = numbered_index_sequence_t<indices>;
  static constexpr auto index_map = integer_sequence_to_array(indices_n{});
  static constexpr auto size = indices_n::size();

  static constexpr void for_all(auto&& func) noexcept
  {
    if constexpr (size > 0)
    {
      // C++20 : template lambda
      [&func]<typename K, K... Index>(std::integer_sequence<K, Index...>)
      {
        (func(
             field_reflection<Index, boost::pfr::tuple_element_t<Index, T>>{}),
         ...);
      }
      (indices_n{});
    }
  }

  static constexpr void for_nth(int n, auto&& func) noexcept
  {
    // TODO maybe there is some dirty hack to do here with offsetof computations...
    // technically, we ought to be able to compute the address of member "n" at compile time...
    // and reinterpret_cast &fields+addr....

    if constexpr (size > 0)
    {
      [ n, &func ]<typename K, K... Index>(std::integer_sequence<K, Index...>)
      {
        // TODO compare with || logical-or fold ?
        ((void)(Index == n && (func(field_reflection<Index, boost::pfr::tuple_element_t<Index, T>>{}), true)),
         ...);
      }
      (indices_n{});
    }
  }

  static constexpr void for_all(type& fields, auto&& func) noexcept
  {
    if constexpr (size > 0)
    {
      [&func, &
       fields ]<typename K, K... Index>(std::integer_sequence<K, Index...>)
      {
        auto&& ppl = boost::pfr::detail::tie_as_tuple(fields);
        (func(boost::pfr::detail::sequence_tuple::get<Index>(ppl)), ...);
      }
      (indices_n{});
    }
  }

  static constexpr void for_nth(type& fields, int n, auto&& func) noexcept
  {
    // TODO maybe there is some dirty hack to do here with offsetof computations...
    // technically, we ought to be able to compute the address of member "n" at compile time...
    // and reinterpret_cast &fields+addr....

    if constexpr (size > 0)
    {
      [ n, &func, &
        fields ]<typename K, K... Index>(std::integer_sequence<K, Index...>)
      {
        auto&& ppl = boost::pfr::detail::tie_as_tuple(fields);
        // TODO compare with || logical-or fold ?
        ((void)(Index == n && (func(boost::pfr::detail::sequence_tuple::get<Index>(ppl)), true)),
         ...);
      }
      (indices_n{});
    }
  }

  static constexpr void for_all(avnd::dummy fields, auto&& func) noexcept
  { }
  static constexpr void for_nth(avnd::dummy fields, int n, auto&& func) noexcept
  { }
  static constexpr void for_all_unless(avnd::dummy fields, int n, auto&& func) noexcept
  { }
};

/**
 * Utilities to introspect all fields in a struct which match a given predicate
 */
template <typename T, template <typename...> typename P>
struct predicate_introspection
{
  using type = T;
  using indices = typed_index_sequence_t<
      std::make_index_sequence<boost::pfr::tuple_size_v<type>>>;

  template <typename IntT>
  using is_parameter_i = P<boost::pfr::tuple_element_t<IntT::value, type>>;

  using fields = boost::mp11::mp_copy_if<as_tuple<type>, P>;
  using indices_n = numbered_index_sequence_t<
      boost::mp11::mp_copy_if<indices, is_parameter_i>>;
  static constexpr auto index_map = integer_sequence_to_array(indices_n{});
  static constexpr auto size = indices_n::size();

  static constexpr void for_all(auto&& func) noexcept
  {
    if constexpr (size > 0)
    {
      [&func]<typename K, K... Index>(std::integer_sequence<K, Index...>)
      {
        (func(
             field_reflection<Index, boost::pfr::tuple_element_t<Index, T>>{}),
         ...);
      }
      (indices_n{});
    }
  }

  static constexpr void for_nth(int n, auto&& func) noexcept
  {
    // TODO maybe there is some dirty hack to do here with offsetof computations...
    // technically, we ought to be able to compute the address of member "n" at compile time...
    // and reinterpret_cast &fields+addr....

    if constexpr (size > 0)
    {
      [ n, &func ]<typename K, K... Index>(std::integer_sequence<K, Index...>)
      {
        // TODO compare with || logical-or fold ?
        ((void)(Index == n && (func(field_reflection<Index, boost::pfr::tuple_element_t<Index, T>>{}), true)),
         ...);
      }
      (indices_n{});
    }
  }

  template<std::size_t N>
  using nth_element = std::decay_t<decltype(boost::pfr::get<index_map[N]>(type{}))>;

  template<std::size_t N>
  static constexpr auto get(type& unfiltered_fields) noexcept
    -> decltype(auto)
  {
    return boost::pfr::get<index_map[N]>(unfiltered_fields);
  }

  static constexpr void for_all(type& unfiltered_fields, auto&& func) noexcept
  {
    if constexpr (size > 0)
    {
      [&func, &unfiltered_fields ]<typename K, K... Index>(
          std::integer_sequence<K, Index...>)
      {
        auto&& ppl = boost::pfr::detail::tie_as_tuple(unfiltered_fields);
        (func(boost::pfr::detail::sequence_tuple::get<Index>(ppl)), ...);
      }
      (indices_n{});
    }
  }

  // Will stop if an error is encountered (func should return bool)
  static constexpr bool for_all_unless(type& unfiltered_fields, auto&& func) noexcept
  {
    if constexpr (size > 0)
    {
      return [&func, &unfiltered_fields ]<typename K, K... Index>(
            std::integer_sequence<K, Index...>)
      {
        auto&& ppl = boost::pfr::detail::tie_as_tuple(unfiltered_fields);
        return (func(boost::pfr::detail::sequence_tuple::get<Index>(ppl)) && ...);
      }
      (indices_n{});
    }
    else
    {
      return true;
    }
  }

  static constexpr void for_nth(type& fields, int n, auto&& func) noexcept
  {
    if constexpr (size > 0)
    {
      [ n, &func, &
        fields ]<typename K, K... Index>(std::integer_sequence<K, Index...>)
      {
        auto&& ppl = boost::pfr::detail::tie_as_tuple(fields);
        ((void)(Index == n && (func(boost::pfr::detail::sequence_tuple::get<Index>(ppl)), true)),
         ...);
      }
      (indices_n{});
    }
  }

  static constexpr void for_all(avnd::dummy fields, auto&& func) noexcept
  { }
  static constexpr void for_nth(avnd::dummy fields, int n, auto&& func) noexcept
  { }
  static constexpr void for_all_unless(avnd::dummy fields, int n, auto&& func) noexcept
  { }
};

}
