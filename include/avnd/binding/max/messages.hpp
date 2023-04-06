#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/max/atom_iterator.hpp>
#include <avnd/binding/max/helpers.hpp>
#include <avnd/introspection/messages.hpp>

namespace max
{
template <typename T>
struct messages
{
  template <typename M>
  static void
  call_static(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    using refl = avnd::message_reflection<M>;
    constexpr auto f = avnd::message_get_func<M>();
    constexpr auto arg_counts = refl::count;

    if(arg_counts != argc)
    {
      object_error(nullptr, "Error: invalid argument count for call %s: ", name.data());
      return;
    }

    using arg_list_t = typename refl::arguments;

    // Check if all arguments passed are convertible to the expected
    // type of the method:
    const bool can_apply_args
        = [&]<typename... Args, std::size_t... I>(
              boost::mp11::mp_list<Args...>, std::index_sequence<I...>) constexpr {
      return (compatible<Args>(argv[I].a_type) && ...);
    }(arg_list_t{}, std::make_index_sequence<arg_counts>());

    if(!can_apply_args)
    {
      object_error(nullptr, "Error: invalid arguments for call %s: ", name.data());
      return;
    }

    // Call the method
    [&]<typename... Args, std::size_t... I>(
        boost::mp11::mp_list<Args...>, std::index_sequence<I...>) {
      constexpr auto f = avnd::message_get_func<M>();
      if constexpr(std::is_member_function_pointer_v<decltype(f)>)
      {
        if constexpr(requires(M m) { m(convert<Args>(argv[I])...); })
          return M{}(convert<Args>(argv[I])...);
        else if constexpr(requires { (implementation.*f)(convert<Args>(argv[I])...); })
          return (implementation.*f)(convert<Args>(argv[I])...);
      }
      else
        return f(convert<Args>(argv[I])...);
        }(arg_list_t{}, std::make_index_sequence<arg_counts>());
  }

  template <typename M>
  static void
  call_instance(T& implementation, std::string_view name, int argc, t_atom* argv)
  {
    using refl = avnd::message_reflection<M>;
    constexpr auto f = avnd::message_get_func<M>();
    constexpr auto arg_counts = refl::count;

    if(arg_counts != (argc + 1))
    {
      object_error(nullptr, "Error: invalid argument count for call %s: ", name.data());
      return;
    }

    using arg_list_t = typename refl::arguments;

    // Check if all arguments passed are convertible to the expected
    // type of the method:
    const bool can_apply_args
        = [&]<typename... Args, std::size_t... I>(
              boost::mp11::mp_list<T&, Args...>, std::index_sequence<I...>) constexpr {
      return (compatible<Args>(argv[I].a_type) && ...);
    }(arg_list_t{}, std::make_index_sequence<arg_counts - 1>());

    if(!can_apply_args)
    {
      object_error(nullptr, "Error: invalid arguments for call %s: ", name.data());
      return;
    }

    // Call the method
    [&]<typename... Args, std::size_t... I>(
        boost::mp11::mp_list<T&, Args...>, std::index_sequence<I...>) {
      if constexpr(std::is_member_function_pointer_v<decltype(f)>)
      {
        if constexpr(requires(M m) { m(implementation, convert<Args>(argv[I])...); })
          return M{}(implementation, convert<Args>(argv[I])...);
        else if constexpr(requires {
                            (implementation
                             .*f)(implementation, convert<Args>(argv[I])...);
                          })
          return (implementation.*f)(implementation, convert<Args>(argv[I])...);
      }
      else
        return f(implementation, convert<Args>(argv[I])...);
        }(arg_list_t{}, std::make_index_sequence<arg_counts - 1>());
  }

  template <typename M>
  static bool process_message(
      T& implementation, M& field, std::string_view sym, int argc, t_atom* argv)
  {
    if constexpr(!std::is_void_v<avnd::message_reflection<M>>)
    {
      constexpr auto arg_count = avnd::message_reflection<M>::count;
      if constexpr(arg_count == 0)
      {
        call_static<M>(implementation, sym, argc, argv);
      }
      else
      {
        if constexpr(std::is_same_v<avnd::first_message_argument<M>, T&>)
        {
          call_instance<M>(implementation, sym, argc, argv);
        }
        else
        {
          call_static<M>(implementation, sym, argc, argv);
        }
      }
      return true;
    }
    else
    {
      if constexpr(requires {
                     M::func()(implementation, make_atom_iterator(argc, argv));
                   })
      {
        M::func()(implementation, make_atom_iterator(argc, argv));
      }
      else if constexpr(requires { M::func()(make_atom_iterator(argc, argv)); })
      {
        M::func()(make_atom_iterator(argc, argv));
      }
      else if constexpr(requires(M m) {
                          m(implementation, make_atom_iterator(argc, argv));
                        })
      {
        M{}(implementation, make_atom_iterator(argc, argv));
      }
      else if constexpr(requires(M m) { m(make_atom_iterator(argc, argv)); })
      {
        M{}(make_atom_iterator(argc, argv));
      }
      else
      {
        static_assert(std::is_void_v<M>, "func() does not return a viable function");
      }
    }

    return false;
  }

  static bool process_messages(auto& implementation, t_symbol* s, int argc, t_atom* argv)
  {
    if constexpr(avnd::has_messages<T>)
    {
      bool ok = false;
      std::string_view symname = s->s_name;
      avnd::messages_introspection<T>::for_all(
          avnd::get_messages(implementation), [&]<typename M>(M& field) {
            if(ok)
              return;
            if(symname == M::name())
            {
              ok = process_message(implementation.effect, field, symname, argc, argv);
            }
          });
      return ok;
    }
    return false;
  }
};
}
