#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <ossia/detail/config.hpp>
#include <ossia/detail/timer.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/context_functions.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/protocols/oscquery/oscquery_server_asio.hpp>
namespace standalone
{
template <typename T>
struct tag
{
  using type = T;
};

template <typename T>
struct oscquery_mapper
{
  avnd::effect_container<T>& object;

  std::shared_ptr<ossia::net::network_context> m_context;
  ossia::net::generic_device m_dev;

  explicit oscquery_mapper(
      avnd::effect_container<T>& object, std::string name, int osc_port, int ws_port)
      : object{object}
      , m_context{std::make_shared<ossia::net::network_context>()}
      , m_dev{
            std::make_unique<ossia::oscquery_asio::oscquery_server_protocol>(
                m_context, osc_port, ws_port),
            name}
  {
    create_ports();
  }

  template <avnd::parameter Field>
    requires(!avnd::enum_parameter<Field>)
  void setup_control(Field& field, ossia::net::parameter_base& param)
  {
    param.set_value_type(oscr::type_for_arg<decltype(Field::value)>());

    // Set-up the metadata
    if constexpr(avnd::parameter_with_minmax_range<Field>)
    {
      constexpr auto ctl = avnd::get_range<Field>();
      param.set_domain(ossia::make_domain(ctl.min, ctl.max));
      param.set_value(ctl.init);
    }

    param.set_access(ossia::access_mode::BI);

    // Set-up the external callback
    param.add_callback([object = &object, &field](const ossia::value& val) {
      oscr::from_ossia_value(field, val, field.value);

      if_possible(field.update(object.effect));
    });
  }

  template <avnd::enum_parameter Field>
  void setup_control(Field& field, ossia::net::parameter_base& param)
  {
    param.set_value_type(ossia::val_type::STRING);

    static constexpr const auto choices = avnd::get_enum_choices<Field>();
    static constexpr const auto choices_count = choices.size();
    ossia::domain_base<std::string> dom{{choices.begin(), choices.end()}};
    // Set-up the metadata
    param.set_value(dom.values[(int)avnd::get_range<Field>().init]);
    param.set_domain(std::move(dom));
    param.set_access(ossia::access_mode::BI);

    // Set-up the external callback
    param.add_callback([object = &object, &field](const ossia::value& val) {
      oscr::from_ossia_value(field, val, field.value);
      if_possible(field.update(object.effect));
    });
  }

  template <avnd::parameter Field>
  void create_control(Field& field)
  {
    ossia::net::node_base& node = m_dev.get_root_node();
    std::string name{avnd::get_path(field)};

    if(auto param
       = ossia::net::create_parameter<ossia::net::generic_parameter>(node, name))
    {
      setup_control(field, *param);
    }
  }

  template <typename Arg>
  static bool compatible(ossia::val_type type)
  {
    if constexpr(requires(Arg arg) { arg = "str"; })
      return type == ossia::val_type::STRING;
    else if constexpr(requires(Arg arg) { arg = std::array<float, 2>{}; })
      return type == ossia::val_type::VEC2F;
    else if constexpr(requires(Arg arg) { arg = std::array<float, 3>{}; })
      return type == ossia::val_type::VEC3F;
    else if constexpr(requires(Arg arg) { arg = std::array<float, 4>{}; })
      return type == ossia::val_type::VEC4F;

    else if constexpr(std::is_same_v<Arg, bool>)
      return type == ossia::val_type::FLOAT || type == ossia::val_type::INT
             || type == ossia::val_type::BOOL;
    else if constexpr(std::is_same_v<Arg, char>)
      return type == ossia::val_type::FLOAT || type == ossia::val_type::INT
             || type == ossia::val_type::BOOL;

    else if constexpr(std::floating_point<Arg>)
      return type == ossia::val_type::FLOAT || type == ossia::val_type::INT
             || type == ossia::val_type::BOOL;
    else if constexpr(std::integral<Arg>)
      return type == ossia::val_type::FLOAT || type == ossia::val_type::INT
             || type == ossia::val_type::BOOL;

    return false;
  }

  template <typename Arg>
  static Arg convert(const ossia::value& atom, tag<Arg>)
  {
    Arg a;
    oscr::from_ossia_value(atom, a);
    return a;
  }

  static std::string convert(const ossia::value& atom, tag<const char*>)
  {
    return ossia::convert<std::string>(atom);
  }

  template <typename Arg>
  static Arg& deref(Arg& self, tag<Arg>) noexcept
  {
    return self;
  }
  static std::string_view deref(std::string& self, tag<std::string_view>) noexcept
  {
    return self;
  }
  static const char* deref(std::string& self, tag<const char*>) noexcept
  {
    return self.c_str();
  }

  template <auto f, typename... Args>
  void call_message_impl(Args&&... args)
  {
    if constexpr(std::is_member_function_pointer_v<decltype(f)>)
      return (object.effect.*f)(std::forward<Args>(args)...);
    else
      return f(std::forward<Args>(args)...);
  }

  template <auto f>
  void call_message_1_arg(const ossia::value& in)
  {
    using arg_list_t = typename avnd::function_reflection<f>::arguments;
    using arg_t = typename avnd::first_argument<f>;
    if constexpr(std::floating_point<arg_t>)
    {
      call_message_impl<f>(ossia::convert<float>(in));
    }
    else if constexpr(std::integral<arg_t>)
    {
      call_message_impl<f>(ossia::convert<int>(in));
    }
    else if constexpr(std::is_same_v<arg_t, bool>)
    {
      call_message_impl<f>(ossia::convert<bool>(in));
    }
    else if constexpr(std::is_same_v<arg_t, const char*>)
    {
      call_message_impl<f>(ossia::convert<std::string>(in).c_str());
    }
    else if constexpr(std::is_same_v<arg_t, std::string>)
    {
      call_message_impl<f>(ossia::convert<std::string>(in));
    }
    else if constexpr(std::is_same_v<arg_t, std::array<float, 2>>)
    {
      call_message_impl<f>(ossia::convert<std::array<float, 2>>(in));
    }
    else if constexpr(std::is_same_v<arg_t, std::array<float, 3>>)
    {
      call_message_impl<f>(ossia::convert<std::array<float, 3>>(in));
    }
    else if constexpr(std::is_same_v<arg_t, std::array<float, 4>>)
    {
      call_message_impl<f>(ossia::convert<std::array<float, 4>>(in));
    }
  }

  template <auto f>
  void call_message_1_arg_self(const ossia::value& in)
  {
    using arg_list_t = typename avnd::function_reflection<f>::arguments;
    using arg_t = typename avnd::first_argument<f>;
    if constexpr(std::floating_point<arg_t>)
    {
      f(object.effect, ossia::convert<float>(in));
    }
    else if constexpr(std::integral<arg_t>)
    {
      f(object.effect, ossia::convert<int>(in));
    }
    else if constexpr(std::is_same_v<arg_t, bool>)
    {
      f(object.effect, ossia::convert<bool>(in));
    }
    else if constexpr(std::is_same_v<arg_t, const char*>)
    {
      f(object.effect, ossia::convert<std::string>(in).c_str());
    }
    else if constexpr(std::is_same_v<arg_t, std::string>)
    {
      f(object.effect, ossia::convert<std::string>(in));
    }
    else if constexpr(std::is_same_v<arg_t, std::string_view>)
    {
      f(object.effect, ossia::convert<std::string>(in));
    }
    else if constexpr(std::is_same_v<arg_t, std::array<float, 2>>)
    {
      f(object.effect, ossia::convert<std::array<float, 2>>(in));
    }
    else if constexpr(std::is_same_v<arg_t, std::array<float, 3>>)
    {
      f(object.effect, ossia::convert<std::array<float, 3>>(in));
    }
    else if constexpr(std::is_same_v<arg_t, std::array<float, 4>>)
    {
      f(object.effect, ossia::convert<std::array<float, 4>>(in));
    }
  }

  template <auto f>
  void call_message_n_arg_impl(const std::vector<ossia::value>& argv)
  {
    constexpr auto arg_counts = avnd::function_reflection<f>::count;
    using arg_list_t = typename avnd::function_reflection<f>::arguments;

    // Check if all arguments passed are convertible to the expected
    // type of the method:
    auto can_apply_args = [&]<typename... Args, std::size_t... I>(
                              boost::mp11::mp_list<Args...>, std::index_sequence<I...>) {
      return (compatible<Args>(argv[I].get_type()) && ...);
    }(arg_list_t{}, std::make_index_sequence<arg_counts>());

    if(!can_apply_args)
    {
      ossia::logger().error("Error: invalid argument count for call.");
      return;
    }

    // Call the method
    [&]<typename... Args, std::size_t... I>(
        boost::mp11::mp_list<Args...>, std::index_sequence<I...>) {
      std::tuple args{convert(argv[I], tag<Args>{})...};
      call_message_impl<f>(deref(get<I>(args), tag<Args>{})...);
        }(arg_list_t{}, std::make_index_sequence<arg_counts>());
  }

  template <auto f>
  void call_message_n_arg(const ossia::value& in)
  {
    constexpr auto arg_counts = avnd::function_reflection<f>::count;
    using arg_list_t = typename avnd::function_reflection<f>::arguments;

    if(in.get_type() != ossia::val_type::LIST)
      return;
    auto& lst = *in.target<std::vector<ossia::value>>();

    if(arg_counts != lst.size())
    {
      ossia::logger().error("Error: invalid argument count for call.");
      return;
    }

    call_message_n_arg_impl<f>(lst);
  }

  template <auto f>
  void call_message(const ossia::value& in)
  {
    constexpr auto arg_counts = avnd::function_reflection<f>::count;
    using arg_list_t = typename avnd::function_reflection<f>::arguments;

    if constexpr(arg_counts == 0)
    {
      call_message_impl<f>();
    }
    else
    {
      if constexpr(std::is_same_v<avnd::first_argument<f>, T&>)
      {
        if constexpr(arg_counts == 1)
        {
          call_message_1_arg_self<f>(in);
        }
        else
        {
          // TODO call_message_n_arg_self<f>(in);
        }
      }
      else
      {
        if constexpr(arg_counts == 1)
        {
          call_message_1_arg<f>(in);
        }
        else
        {
          call_message_n_arg<f>(in);
        }
      }
    }
  }

  template <typename... Args>
  void init_message_arguments(
      ossia::net::parameter_base& param, boost::mp11::mp_list<Args...>)
  {
    if constexpr(sizeof...(Args) == 0)
    {
      param.set_value_type(ossia::val_type::IMPULSE);
    }
    else if constexpr(sizeof...(Args) == 1)
    {
      param.set_value_type(oscr::type_for_arg<Args...>());
    }
    else
    {
      std::vector<ossia::value> init;
      init.reserve(sizeof...(Args));
      param.set_value_type(ossia::val_type::LIST);

      (init.push_back(ossia::init_value(oscr::type_for_arg<Args>())), ...);
      param.set_value(std::move(init));
    }
  }

  template <typename... Args>
  void init_message_arguments(
      ossia::net::parameter_base& param, boost::mp11::mp_list<T&, Args...>)
  {
    if constexpr(sizeof...(Args) == 0)
    {
      param.set_value_type(ossia::val_type::IMPULSE);
    }
    else if constexpr(sizeof...(Args) == 1)
    {
      param.set_value_type(type_for_arg<Args...>());
    }
    else
    {
      std::vector<ossia::value> init;
      init.reserve(sizeof...(Args));
      param.set_value_type(ossia::val_type::LIST);

      (init.push_back(ossia::init_value(type_for_arg<Args>())), ...);
      param.set_value(std::move(init));
    }
  }

  template <typename Field>
  void create_message(Field& field)
  {
    if constexpr(requires { avnd::function_reflection<Field::func()>::count; })
    {
      ossia::net::node_base& node = m_dev.get_root_node();
      std::string name{avnd::get_path(field)};
      if(auto param = ossia::net::create_parameter<ossia::net::generic_parameter>(
             node, name)) // TODO
      {
        // Set-up the parameter
        using arg_list_t = typename avnd::function_reflection<Field::func()>::arguments;
        init_message_arguments(*param, arg_list_t{});

        // Set-up the external callback
        param->add_callback([this, &field](auto& val) {
          if constexpr(requires { avnd::function_reflection<Field::func()>::count; })
          {
            call_message<Field::func()>(val);
            return true;
          }
          else
          {
            static_assert(std::is_void_v<Field>, "func() does not return a function");
          }
        });
      }
    }
  }

  template <avnd::parameter Field>
  void create_output(Field& field)
  {
    ossia::net::node_base& node = m_dev.get_root_node();
    std::string name{avnd::get_path(field)};

    if(auto param
       = ossia::net::create_parameter<ossia::net::generic_parameter>(node, name))
    {
      param->set_value_type(oscr::type_for_arg<decltype(Field::value)>());
      if constexpr(avnd::has_range<Field>)
      {
        constexpr auto ctl = avnd::get_range<Field>();
        param->set_domain(ossia::make_domain(ctl.min, ctl.max));
      }

      param->set_access(ossia::access_mode::GET);
    }
  }

  void create_ports()
  {
    if constexpr(avnd::has_inputs<T>)
      avnd::parameter_input_introspection<T>::for_all(
          avnd::get_inputs<T>(object),
          [this]<typename Field>(Field& f) { create_control(f); });

    if constexpr(avnd::has_outputs<T>)
      avnd::parameter_output_introspection<T>::for_all(
          avnd::get_outputs<T>(object),
          [this]<typename Field>(Field& f) { create_output(f); });

    if constexpr(avnd::has_messages<T>)
      avnd::messages_introspection<T>::for_all(
          avnd::get_messages<T>(object),
          [&]<typename Field>(Field& f) { create_message(f); });

    // FIXME: for callbacks we must not overwrite the callback already set
    // by the binding.
    // Same thing for the UI.
    // -> in the end, we need to have some multiplexing layer for when we have more than 1 binding
    //
    // if constexpr(avnd::has_outputs<T>)
    //   avnd::callback_output_introspection<T>::for_all(
    //       avnd::get_outputs<T>(object),
    //       [&]<typename Field>(Field& f) { create_callback(f); });
  }

  void run() { m_context->run(); }
  void stop() { m_context->context.stop(); }
};
}
