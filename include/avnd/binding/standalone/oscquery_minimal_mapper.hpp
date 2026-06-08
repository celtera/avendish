#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <ossia/detail/config.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/context_functions.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/protocols/oscquery/oscquery_server_asio.hpp>

#include <iostream>
#include <thread>
#include <chrono>
#if __has_include(<jthread>)
#include <jthread>
#endif
namespace standalone
{
template <typename T>
struct oscquery_minimal_mapper
{
  using inputs_type = typename avnd::inputs_type<T>::type;
  std::shared_ptr<ossia::net::network_context> m_context;
  ossia::net::generic_device m_dev;
  std::jthread m_thread;

  explicit oscquery_minimal_mapper(std::string name, int osc_port, int ws_port)
      : m_context{std::make_shared<ossia::net::network_context>()}
      , m_dev{
            std::make_unique<ossia::oscquery_asio::oscquery_server_protocol>(
                m_context),
            name}
  {
    m_thread = std::jthread([this] (std::stop_token tk){
      std::this_thread::sleep_for(std::chrono::seconds(1));
      while(!tk.stop_requested())
      {
        std::cerr << ("Starting...") << std::endl;
        m_context->run();
      }
        std::cerr << ("Exited") << std::endl;
    });
  }

  ~oscquery_minimal_mapper()
  {
    std::cerr <<("oh shit");
  }

  template <avnd::parameter Field>
    requires(!avnd::enum_parameter<Field>)
  void setup_control(Field& field, ossia::net::parameter_base& param)
  {
    std::cerr << ("Adding a control") << avnd::get_name<Field>() << std::endl;
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
    param.add_callback([&field](const ossia::value& val) {
      oscr::from_ossia_value(field, val, field.value);
      // FIXME callback into the origin
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
    param.add_callback([&field](const ossia::value& val) {
      oscr::from_ossia_value(field, val, field.value);
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

  void create_ports(typename avnd::inputs_type<T>::type& inputs)
  {
    avnd::parameter_input_introspection<T>::for_all(inputs,
                                                    [this]<typename Field>(Field& f) { create_control(f); });
  }
};
}
