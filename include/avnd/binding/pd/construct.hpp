#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/pd/atom_iterator.hpp>
#include <avnd/binding/pd/helpers.hpp>
#include <avnd/concepts/generic.hpp>
#include <avnd/concepts/object.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/variant2/variant.hpp>

namespace pd
{
template <typename T>
constexpr auto get_possible_constructor_types()
{
  using namespace std;
  if constexpr(requires { avnd::type_list<decltype(T::construct)>; })
  {
    if constexpr(avnd::type_list<decltype(T::construct)>)
    {
      AVND_STATIC_TODO(T);
      return avnd::typelist<>{};
    }
  }

  if constexpr(requires { decltype(&T::construct){}; })
  {
    constexpr auto f
        = []<typename R, typename... Args>(R (*f)(Args...)) -> R* { return nullptr; };
    if constexpr(requires { f(&T::construct); })
    {
      // Single function
      using res = std::remove_pointer_t<decltype(f(&T::construct))>;
      if constexpr(avnd::variant_ish<res>)
      {
        return boost::mp11::mp_rename<res, avnd::typelist>{};
      }
      else
      {
        return avnd::typelist<res>{};
      }
    }
    else
    {
      constexpr auto f = []<typename R, typename... Args>(R (T::*f)(Args...)) -> R* {
        return nullptr;
      };
      if constexpr(requires { f(&T::construct); })
      {
        // Single function
        using res = std::remove_pointer_t<decltype(f(&T::construct))>;
        if constexpr(avnd::variant_ish<res>)
        {
          return boost::mp11::mp_rename<res, avnd::typelist>{};
        }
        else
        {
          return avnd::typelist<res>{};
        }
      }
      else
      {
        AVND_STATIC_TODO(T);
        return avnd::typelist<>{};
      }
    }
  }
  else
  {
    // Template function taking an input range of string / numbers
    std::span<std::variant<float, std::string_view>> dummy;
    using res = std::remove_cvref_t<std::decay_t<decltype(T::construct(dummy))>>;
    if constexpr(avnd::variant_ish<res>)
    {
      return boost::mp11::mp_rename<res, avnd::typelist>{};
    }
    else
    {
      return avnd::typelist<res>{};
    }
  }
}

template <typename T>
using constructor_type_list = boost::mp11::mp_unique<
    std::remove_cvref_t<decltype(get_possible_constructor_types<T>())>>;
template <typename T>
using constructor_variant
    = boost::mp11::mp_rename<constructor_type_list<T>, boost::variant2::variant>;

template <typename... Args>
constexpr std::size_t max_sizeof(avnd::typelist<Args...>) noexcept
{
  static_assert(sizeof...(Args) > 0);
  return std::max(std::initializer_list<std::size_t>{sizeof(Args)...});
}

#define KEYWORD construct
// FIXME implement attribute support
// -> semi-manual way in halp::attributes but we could also have an automatic way...
template <typename T>
struct construct_arguments
{
  static auto call_noargs(T& implementation)
  {
    constexpr auto f = &T::KEYWORD;
    if constexpr(std::is_member_function_pointer_v<decltype(f)>)
      return (implementation.*f)();
    else
      return f();
  }

  static auto call_vec(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    boost::container::small_vector<std::variant<float, std::string_view>, 64> ctor;

    for(int i = 0; i < argc; i++)
    {
      if(argv[i].a_type == t_atomtype::A_FLOAT)
        ctor.emplace_back(argv[i].a_w.w_float);
      else if(argv[i].a_type == t_atomtype::A_SYMBOL)
        ctor.emplace_back(argv[i].a_w.w_symbol->s_name);
    }

    return implementation.KEYWORD(ctor);
  }

  static auto call_span(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    return call_vec(implementation, name, argc, argv);
  }

  static auto
  call_coroutine(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    if constexpr(requires { implementation.KEYWORD(make_atom_iterator(argc, argv)); })
    {
      return implementation.KEYWORD(make_atom_iterator(argc, argv));
    }
    else
    {
      auto iterator = make_atom_iterator(argc, argv);
      if constexpr(requires { implementation.KEYWORD(implementation, iterator); })
        return implementation.KEYWORD(implementation, iterator);
      else
        AVND_STATIC_TODO(T);
    }
  }

  static auto
  call_static(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    constexpr auto f = &T::KEYWORD;
    constexpr auto arg_counts = avnd::function_reflection<&T::KEYWORD>::count;

    if(arg_counts != argc)
    {
      startpost("Error: invalid argument count for call %s: ", name.data());
      postatom(argc, argv);
      endpost();
      return;
    }

    using arg_list_t = typename avnd::function_reflection<f>::arguments;

    // Check if all arguments passed are convertible to the expected
    // type of the method:
    const bool can_apply_args
        = [&]<typename... Args, std::size_t... I>(
              boost::mp11::mp_list<Args...>, std::index_sequence<I...>) constexpr {
      return (compatible<Args>(argv[I].a_type) && ...);
    }(arg_list_t{}, std::make_index_sequence<arg_counts>());

    if(!can_apply_args)
    {
      startpost("Error: invalid arguments for call %s: ", name.data());
      postatom(argc, argv);
      endpost();
      return;
    }

    // Call the method
    [&]<typename... Args, std::size_t... I>(
        boost::mp11::mp_list<Args...>, std::index_sequence<I...>) {
      implementation.KEYWORD(convert<Args>(argv[I])...);
        }(arg_list_t{}, std::make_index_sequence<arg_counts>());
  }

  static auto
  call_simple(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    constexpr auto f = &T::KEYWORD;
    constexpr auto arg_counts = avnd::function_reflection<&T::KEYWORD>::count;

    if constexpr(arg_counts == 0)
    {
      // Let's still call the function
      return call_noargs(implementation);
    }
    else if constexpr(arg_counts == 1)
    {
      using main_arg_type = avnd::first_argument<&T::KEYWORD>;
      // If it's a std::vector<std::variant<...>>
      if constexpr(avnd::vector_ish<main_arg_type>)
      {
        return call_vec(implementation, name, argc, argv);
      }
      else if constexpr(avnd::iterable_ish<main_arg_type>)
      {
        return call_span(implementation, name, argc, argv);
      }
      else if constexpr(requires {
                          implementation.KEYWORD(make_atom_iterator(argc, argv));
                        })
      {
        return implementation.KEYWORD(make_atom_iterator(argc, argv));
      }
      else
      {
        return call_static(implementation, name, argc, argv);
      }
    }
    else
    {
      return call_static(implementation, name, argc, argv);
    }
  }

  template <typename F>
  static auto
  call_instance(F f, T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    constexpr auto arg_counts = avnd::function_reflection<decltype(+f){}>::count;

    using arg_list_t = typename avnd::function_reflection<decltype(+f){}>::arguments;

    // Check if all arguments passed are convertible to the expected
    // type of the method (we expect that the first argument is a T&:
    const bool can_apply_args
        = [&]<typename... Args, std::size_t... I>(
              boost::mp11::mp_list<T&, Args...>, std::index_sequence<I...>) constexpr {
      return (compatible<Args>(argv[I].a_type) && ...);
    }(arg_list_t{}, std::make_index_sequence<arg_counts - 1>());

    if(!can_apply_args)
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
      F f, T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    constexpr auto arg_counts = avnd::function_reflection<decltype(+f){}>::count;
    if(arg_counts != (argc + 1))
      return false;

    if constexpr(arg_counts == 0)
    {
      call_noargs(implementation);
      return true;
    }
    else if constexpr(arg_counts == 1)
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
        [&]<typename... Args>(const Args&... args) {
      (call_overloaded_impl(args, implementation, name, argc, argv) || ...);
        },
        T::KEYWORD);
  }

  static void
  call_template(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    // Here we will have a coroutine which will iterate across our arguments
    if_possible(implementation.KEYWORD(make_atom_iterator(argc, argv)));
  }

  static auto call(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    using namespace std;
    if constexpr(requires { avnd::type_list<decltype(T::KEYWORD)>; })
    {
      if constexpr(avnd::type_list<decltype(T::KEYWORD)>)
      {
        return call_overloaded(implementation, name, argc, argv);
      }
      else if constexpr(requires { decltype(&T::KEYWORD){}; })
      {
        return call_simple(implementation, name, argc, argv);
      }
      else
      {
        return call_template(implementation, name, argc, argv);
      }
    }
    else if constexpr(requires { decltype(&T::KEYWORD){}; })
    {
      return call_simple(implementation, name, argc, argv);
    }
    else
    {
      return call_template(implementation, name, argc, argv);
    }
  }

  auto process(T& implementation, int argc, t_atom* argv)
  {
    return call(implementation, "<KEYWORD>", argc, argv);
  }
};

template <typename T>
void invoke_construct(t_symbol* s, int argc, t_atom* argv, auto func)
{
  using namespace std;
  T t_ctor{};
  auto v = construct_arguments<T>{}.process(t_ctor, argc, argv);
  std::visit(
      [&](auto&& obj) { func(std::move(obj)); },
      construct_arguments<T>{}.process(t_ctor, argc, argv));
}
}
