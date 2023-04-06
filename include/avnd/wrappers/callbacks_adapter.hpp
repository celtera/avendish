#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/callback.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>

namespace avnd
{

/**
 * We store an array of std::function objects corresponding to the
 * types of each function_view-ish callback
 * (callbacks defined as a function pointer / context pointer pair)
 */
template <typename T>
struct callback_storage_views
{
};

template <typename... Args>
struct view_callback_function_type;
template <typename R, typename... Args>
struct view_callback_function_type<R(void*, Args...)>
{
  using type = R(Args...);
};

template <avnd::view_callback Field>
using view_callback_message_type = typename view_callback_function_type<
    std::remove_pointer_t<std::decay_t<decltype(Field{}.call.function)>>>::type;

template <typename T>
  requires(
      avnd::view_callback_introspection<typename avnd::outputs_type<T>::type>::size > 0)
struct callback_storage_views<T>
{
  using tuple = filter_and_apply<
      view_callback_message_type, view_callback_introspection,
      typename avnd::outputs_type<T>::type>;
  using vectors = boost::mp11::mp_transform<std::function, tuple>;

  [[no_unique_address]] vectors functions_storage;
};

/**
 * Used to allocate the data for the callbacks when necessary
 */
template <typename T>
struct callback_storage : callback_storage_views<T>
{
  void wrap_callbacks(avnd::effect_container<T>& effect, auto callback_handler)
  {
    using outputs_t = typename avnd::outputs_type<T>::type;

    if constexpr(dynamic_callback_introspection<outputs_t>::size > 0)
    {
      auto setup_dyn = [callback_handler]<auto Idx, auto IdxGlob, typename C>(
          C & cb, avnd::predicate_index<Idx>, avnd::field_index<IdxGlob>)
      {
        using call_type = decltype(C::call);
        using func_type = decltype(cb.call);
        using func_reflect = avnd::function_reflection_t<func_type>;
        using ret = typename func_reflect::return_type;
        using args = typename func_reflect::arguments;

        using namespace boost::mp11;
        using typelist = mp_push_front<args, ret>;

        // Here, "cb.call" is something like std::function,
        // thus we can store the callback directly.
        cb.call = callback_handler(cb, typelist{}, avnd::num<IdxGlob>{});
      };

      avnd::dynamic_callback_introspection<outputs_t>::for_all_n2(
          effect.outputs(), setup_dyn);
    }

    if constexpr(view_callback_introspection<outputs_t>::size > 0)
    {
      auto setup_view = [ this, callback_handler ]<auto Idx, auto IdxGlob, typename C>(
          C & cb, avnd::predicate_index<Idx>, avnd::field_index<IdxGlob>)
      {
        using namespace tpl;
        using call_type = decltype(C::call);

        // Generate a dummy function if we don't have anything to bind it to.
        using func_type = decltype(*cb.call.function);
        using func_reflect = avnd::function_reflection_t<func_type>;
        using ret = typename func_reflect::return_type;
        using args = typename func_reflect::arguments;

        using namespace boost::mp11;
        // We neeed to remove the first void* argument to the callback:
        using typelist = mp_push_front<mp_pop_front<args>, ret>;

        auto& buf = tpl::get<Idx>(this->functions_storage);
        using stored_type
            = std::tuple_element_t<Idx, std::decay_t<decltype(this->functions_storage)>>;
        buf = callback_handler(cb, typelist{}, avnd::num<IdxGlob>{});

        // This version does not allocate, it's a plain old C callback.
        // Thus we store it outside...

        // So, here's there's a bit of C++ trickery.
        // We cannot just do :
        //   cb.call.function = [] (auto&&...) { call the function }
        // as it is not possible to assign a generic lambda to a plain C function pointer.
        // By unrolling the (known) arguments through a first level of lambda,
        // we get "proper" types as arguments to the lambda function which allows
        // us to make it work with unary operator+ which transforms the lambda into a function pointer

#if !defined(_MSC_VER)
        cb.call.function =
            []<template <typename...> typename L, typename... Args>(L<void*, Args...>) {
          // this is what actually goes in cb.call.function:
          return +[](void* self, Args... args) {
            (*reinterpret_cast<stored_type*>(self))(args...);
          };
        }(args{}); // < note that the top-level lambda is immediately invoked here !

        cb.call.context = &buf;
#endif
      };

      avnd::view_callback_introspection<outputs_t>::for_all_n2(
          effect.outputs(), setup_view);
    }
  }
};
}
