#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/pd/atom_iterator.hpp>
#include <avnd/binding/pd/helpers.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/concepts/object.hpp>

namespace pd
{

template <typename T>
struct init_arguments
{
  static void call_noargs(T& implementation)
  {
    constexpr auto f = &T::initialize;
    if constexpr (std::is_member_function_pointer_v<decltype(f)>)
      return (implementation.*f)();
    else
      return f();
  }

  static void call_vec(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    // TODO std::vector<variant<float, string>>
  }

  static void call_span(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    // TODO avnd::span<variant<float, string>>
  }

  static void
  call_coroutine(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    auto iterator = make_atom_iterator(argc, argv);

    if constexpr (requires {
                    implementation.initialize(make_atom_iterator(argc, argv));
                  })
      return implementation.initialize(make_atom_iterator(argc, argv));
    else if constexpr (requires { implementation.initialize(implementation, iterator); })
      return implementation.initialize(implementation, iterator);
    else
      STATIC_TODO(T);
  }

  static void
  call_static(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    constexpr auto f = &T::initialize;
    constexpr auto arg_counts = avnd::function_reflection<&T::initialize>::count;

    if (arg_counts != argc)
    {
      startpost("Error: invalid argument count for call %s: ", name.data());
      postatom(argc, argv);
      endpost();
      return;
    }

    using arg_list_t = typename avnd::function_reflection<f>::arguments;

    // Check if all arguments passed are convertible to the expected
    // type of the method:
    const bool can_apply_args = [&]<typename... Args, std::size_t... I>(
        boost::mp11::mp_list<Args...>, std::index_sequence<I...>) constexpr
    {
      return (compatible<Args>(argv[I].a_type) && ...);
    }
    (arg_list_t{}, std::make_index_sequence<arg_counts>());

    if (!can_apply_args)
    {
      startpost("Error: invalid arguments for call %s: ", name.data());
      postatom(argc, argv);
      endpost();
      return;
    }

    // Call the method
    [&]<typename... Args, std::size_t... I>(
        boost::mp11::mp_list<Args...>, std::index_sequence<I...>) {
      implementation.initialize(convert<Args>(argv[I])...);
    }(arg_list_t{}, std::make_index_sequence<arg_counts>());
  }

  static void
  call_simple(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    constexpr auto f = &T::initialize;
    constexpr auto arg_counts = avnd::function_reflection<&T::initialize>::count;

    if constexpr (arg_counts == 0)
    {
      // Let's still call the function
      return call_noargs(implementation);
    }
    else if constexpr (arg_counts == 1)
    {
      using main_arg_type = avnd::first_argument<&T::initialize>;
      // If it's a std::vector<std::variant<...>>
      if constexpr (avnd::vector_ish<main_arg_type>)
      {
        call_vec(implementation, name, argc, argv);
        return;
      }
      else if constexpr (avnd::span_ish<main_arg_type>)
      {
        call_span(implementation, name, argc, argv);
        return;
      }
      else if constexpr (requires {
                           implementation.initialize(make_atom_iterator(argc, argv));
                         })
      {
        implementation.initialize(make_atom_iterator(argc, argv));
        return;
      }
      else
      {
        call_static(implementation, name, argc, argv);
        return;
      }
    }
    else
    {
      call_static(implementation, name, argc, argv);
      return;
    }
  }

  template <typename F>
  static bool
  call_instance(F f, T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    constexpr auto arg_counts = avnd::function_reflection<decltype(+f){}>::count;

    using arg_list_t = typename avnd::function_reflection<decltype(+f){}>::arguments;

    // Check if all arguments passed are convertible to the expected
    // type of the method (we expect that the first argument is a T&:
    const bool can_apply_args = [&]<typename... Args, std::size_t... I>(
        boost::mp11::mp_list<T&, Args...>, std::index_sequence<I...>) constexpr
    {
      return (compatible<Args>(argv[I].a_type) && ...);
    }
    (arg_list_t{}, std::make_index_sequence<arg_counts - 1>());

    if (!can_apply_args)
    {
      return false;
    }

    // Call the method
    [&]<typename... Args, std::size_t... I>(
        boost::mp11::mp_list<T&, Args...>, std::index_sequence<I...>) {
      return f(implementation, convert<Args>(argv[I])...);
    }(arg_list_t{}, std::make_index_sequence<arg_counts - 1>());

    return true;
  }

  template <typename F>
  static bool call_overloaded_impl(
      F f,
      T& implementation,
      std::string_view name,
      int argc,
      t_atom* argv)
  {
    constexpr auto arg_counts = avnd::function_reflection<decltype(+f){}>::count;
    if (arg_counts != (argc + 1))
      return false;

    if constexpr (arg_counts == 0)
    {
      call_noargs(implementation);
      return true;
    }
    else if constexpr (arg_counts == 1)
    {
      f(implementation);
      return true;
    }
    else
    {
      return call_instance(f, implementation, name, argc, argv);
    }
  }

  static void
  call_overloaded(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    // Generate an array with the required types for the overload set.
    std::apply(
        [&]<typename... Args>(const Args&... args)
        { (call_overloaded_impl(args, implementation, name, argc, argv) || ...); },
        T::initialize);
  }

  static void
  call_template(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    // Here we will have a coroutine which will iterate across our arguments
    if_possible(implementation.initialize(make_atom_iterator(argc, argv)));
  }

  static void call(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    using namespace std;
    if constexpr (requires { avnd::type_list<decltype(T::initialize)>; })
    {
      if constexpr (avnd::type_list<decltype(T::initialize)>)
      {
        call_overloaded(implementation, name, argc, argv);
      }
    }
    else if constexpr (requires { decltype(&T::initialize){}; })
    {
      call_simple(implementation, name, argc, argv);
    }
    else
    {
      call_template(implementation, name, argc, argv);
    }
  }

  void process(T& implementation, int argc, t_atom* argv)
  {
    call(implementation, "<initialize>", argc, argv);
  }
};

}
