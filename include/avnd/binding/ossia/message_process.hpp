#pragma once
#include <avnd/common/aggregates.hpp>
#include <avnd/concepts/all.hpp>
#include <boost/mp11.hpp>
#include <ossia/network/value/value.hpp>

#include <optional>
#include <tuple>

namespace oscr
{

template <typename T>
struct message_processor
{
  template <typename M, typename Args, std::size_t Idx>
  std::optional<Args> value_to_argument_tuple(auto& self, const ossia::value& val)
  {
    static constexpr std::size_t count = boost::mp11::mp_size<Args>{};

    if constexpr(count == 0)
    {
      return std::tuple<>{};
    }
    else if constexpr(count == 1)
    {
      static constexpr M field;
      typename boost::mp11::mp_front<Args> arg;
      self.from_ossia_value(field, val, arg, avnd::field_index<Idx>{});
      return std::make_tuple(arg);
    }
    else
    {
      static constexpr bool all_numbers
          = boost::mp11::mp_all_of<Args, std::is_integral>{}
            || boost::mp11::mp_all_of<Args, std::is_floating_point>{};
      switch(val.get_type())
      {
        case ossia::val_type::VEC2F: {
          if constexpr(count == 2 && all_numbers)
          {
            const auto& vec = *val.target<ossia::vec2f>();
            return std::make_tuple(vec[0], vec[1]);
          }
          break;
        }
        case ossia::val_type::VEC3F:
          if constexpr(count == 3 && all_numbers)
          {
            const auto& vec = *val.target<ossia::vec3f>();
            return std::make_tuple(vec[0], vec[1], vec[2]);
          }
          break;
        case ossia::val_type::VEC4F:
          if constexpr(count == 4 && all_numbers)
          {
            const auto& vec = *val.target<ossia::vec4f>();
            return std::make_tuple(vec[0], vec[1], vec[2], vec[3]);
          }
          break;
        case ossia::val_type::LIST: {
          static constexpr M field;
          Args tuple;
          const auto& vec = *val.target<std::vector<ossia::value>>();
          if(vec.size() == count)
          {
            std::apply(
                [&self, &vec, it = vec.begin()]<typename... Ts>(Ts&&... args) mutable {
              (self.from_ossia_value(field, *(it++), args, avnd::field_index<Idx>{}),
               ...);
            }, tuple);
            return tuple;
          }
          break;
        }
        case ossia::val_type::MAP: {
          static constexpr M field;
          Args tuple;
          const auto& vec = *val.target<ossia::value_map_type>();
          if(vec.size() == count)
          {
            std::apply(
                [&self, &vec, it = vec.begin()]<typename... Ts>(Ts&&... args) mutable {
              (self.from_ossia_value(
                   field, (it++)->second, args, avnd::field_index<Idx>{}),
               ...);
            }, tuple);
            return tuple;
          }
          break;
        }
        default:
          break;
      }
    }
    return std::nullopt;
  }

  template <typename M, typename... Args>
  void invoke_message_impl(auto& self, const auto& res, auto& obj, Args&&... first_args)
  {
    static constexpr auto f = avnd::message_get_func<M>();
    using F = std::remove_cvref_t<decltype(f)>;

    std::apply(
        [&obj, ... first_args
               = std::forward<Args>(first_args)]<typename... Ts>(Ts&&... args) mutable {
      if constexpr(std::is_member_function_pointer_v<F>)
      {
        // M is a functor ; the function f is its operator()
        if_possible(M{}(std::forward<Args>(first_args)..., std::forward<Ts>(args)...))
            // M contains a function pointer on the root object
            else if_possible(
                (obj.*f)(std::forward<Args>(first_args)..., std::forward<Ts>(args)...));
      }
      else
      {
        // M contains a function pointer to a free function f
        f(std::forward<Args>(first_args)..., std::forward<Ts>(args)...);
      }
    },
        res);
  }

  template <auto Idx, typename M>
  void invoke_message_first_arg_is_object(
      auto& self, const ossia::value& val, int64_t ts, avnd::field_reflection<Idx, M>)
  {
    auto& impl = self.impl;
    using refl = avnd::message_reflection<M>;
    using namespace boost::mp11;
    using first_arg_type = std::remove_cvref_t<avnd::first_message_argument<M>>;

    if constexpr(avnd::tag_timestamp<M>)
    {
      if constexpr(std::is_same_v<first_arg_type, T>)
      {
        static_assert(boost::mp11::mp_size<typename refl::arguments>::value >= 2);
        using main_args = mp_pop_front<mp_pop_front<typename refl::arguments>>;
        using no_ref = mp_transform<std::remove_cvref_t, main_args>;
        using args = mp_rename<no_ref, std::tuple>;

        if(auto res = value_to_argument_tuple<M, args, Idx>(self, val))
        {
          for(auto& m : impl.effects())
          {
            invoke_message_impl<M>(self, *res, m, m, ts);
          }
        }
      }
      else
      {
        static_assert(boost::mp11::mp_size<typename refl::arguments>::value >= 1);
        using main_args = mp_pop_front<typename refl::arguments>;
        using no_ref = mp_transform<std::remove_cvref_t, main_args>;
        using args = mp_rename<no_ref, std::tuple>;
        if(auto res = value_to_argument_tuple<M, args, Idx>(self, val))
        {
          for(auto& m : impl.effects())
          {
            invoke_message_impl<M>(self, *res, m, ts);
          }
        }
      }
    }
    else
    {
      if constexpr(std::is_same_v<first_arg_type, T>)
      {
        static_assert(boost::mp11::mp_size<typename refl::arguments>::value >= 1);
        using main_args = mp_pop_front<typename refl::arguments>;
        using no_ref = mp_transform<std::remove_cvref_t, main_args>;
        using args = mp_rename<no_ref, std::tuple>;

        if(auto res = value_to_argument_tuple<M, args, Idx>(self, val))
        {
          for(auto& m : impl.effects())
          {
            invoke_message_impl<M>(self, *res, m, m);
          }
        }
      }
      else
      {
        using main_args = typename refl::arguments;
        using no_ref = mp_transform<std::remove_cvref_t, main_args>;
        using args = mp_rename<no_ref, std::tuple>;
        if(auto res = value_to_argument_tuple<M, args, Idx>(self, val))
        {
          for(auto& m : impl.effects())
          {
            invoke_message_impl<M>(self, *res, m);
          }
        }
      }
    }
  }

  template <auto Idx, typename M>
  void invoke_message(
      auto& self, const ossia::value& val, int64_t ts,
      avnd::field_reflection<Idx, M> idx)
  {
    auto& impl = self.impl;
    if constexpr(!std::is_void_v<avnd::message_reflection<M>>)
    {
      using refl = avnd::message_reflection<M>;

      if constexpr(refl::count == 0)
      {
        for(auto& m : impl.effects())
        {
          invoke_message_impl<M>(self, std::tuple<>{}, m);
        }
      }
      else
      {
        invoke_message_first_arg_is_object(self, val, ts, idx);
      }
    }
  }

  template <auto Idx, typename M>
  void operator()(auto& self, avnd::field_reflection<Idx, M>)
  {
    ossia::value_inlet& inl = self.message_ports.message_inlets[Idx];
    if(inl.data.get_data().empty())
      return;
    for(const auto& val : inl.data.get_data())
    {
      invoke_message(self, val.value, val.timestamp, avnd::field_reflection<Idx, M>{});
    }
  }
};
}
