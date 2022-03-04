#pragma once
#include <avnd/common/struct_reflection.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>

#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/graph_node.hpp>

namespace oscr
{

// Compile-time map of avnd concept to ossia port
template <typename T>
struct get_ossia_inlet_type;
template <avnd::audio_port T>
struct get_ossia_inlet_type<T>
{
  using type = ossia::audio_inlet;
};
template <avnd::parameter T>
struct get_ossia_inlet_type<T>
{
  using type = ossia::value_inlet;
};
template <avnd::midi_port T>
struct get_ossia_inlet_type<T>
{
  using type = ossia::midi_inlet;
};
template <avnd::texture_port T>
struct get_ossia_inlet_type<T>
{
  using type = ossia::texture_inlet;
};
template <avnd::message T>
struct get_ossia_inlet_type<T>
{
  using type = ossia::value_inlet;
};
template <avnd::unreflectable_message T>
struct get_ossia_inlet_type<T>
{
    using type = ossia::value_inlet;
};

template <typename T>
using get_ossia_inlet_type_t = typename get_ossia_inlet_type<T>::type;

template <typename T>
struct get_ossia_outlet_type;
template <avnd::audio_port T>
struct get_ossia_outlet_type<T>
{
  using type = ossia::audio_outlet;
};
template <avnd::parameter T>
struct get_ossia_outlet_type<T>
{
  using type = ossia::value_outlet;
};
template <avnd::midi_port T>
struct get_ossia_outlet_type<T>
{
  using type = ossia::midi_outlet;
};
template <avnd::texture_port T>
struct get_ossia_outlet_type<T>
{
  using type = ossia::texture_outlet;
};
template <avnd::callback T>
struct get_ossia_outlet_type<T>
{
  using type = ossia::value_outlet;
};

template <typename T>
using get_ossia_outlet_type_t = typename get_ossia_outlet_type<T>::type;

template <typename T>
struct inlet_storage
{
  // Type of the "inputs" struct:
  // struct inputs {
  //   struct a: audio_in { } a;
  //   struct b: audio_out { } b;
  //   struct c: control_inlet { float value; } c;
  // };
  using inputs_type = typename avnd::inputs_type<T>::type;

  // tuple<a, b, c>
  using inputs_tuple = typename avnd::inputs_type<T>::tuple;

  // tuple<ossia::audio_inlet, ossia::audio_outlet, ossia::value_inlet>
  using ossia_inputs_tuple
      = boost::mp11::mp_transform<get_ossia_inlet_type_t, inputs_tuple>;
  ossia_inputs_tuple ports;
  /*
  // mp_list<mp_size<0>, mp_size<1>, mp_size<2>>;
  using inputs_indices = boost::mp11::mp_iota_c<boost::mp11::mp_size<inputs_tuple>::value>;


  // Is the port at index N, a control input
  template<class N>
  using check_control_input = is_control_input<boost::mp11::mp_at_c<inputs_tuple, N::value>>;

  // mp_list<mp_size<2>>
  using control_input_indices = boost::mp11::mp_copy_if<inputs_indices, check_control_input>;

  // control_input_index<0>::value == 2
  template<std::size_t ControlN>
  using control_input_index = boost::mp11::mp_at_c<control_input_indices, ControlN>;

  // tuple<c>
  using control_input_tuple = boost::mp11::mp_copy_if<inputs_tuple, is_control_input>;

  // tuple<float>
  using control_input_values_type = typename oscr::get_control_type_list<control_input_tuple>::type;

  */
};

template <typename T>
struct outlet_storage
{
  using outputs_type = typename avnd::outputs_type<T>::type;

  using outputs_tuple = typename avnd::outputs_type<T>::tuple;

  using ossia_outputs_tuple
      = boost::mp11::mp_transform<get_ossia_outlet_type_t, outputs_tuple>;
  ossia_outputs_tuple ports;
};


struct setup_value_port
{
  template<avnd::parameter_with_full_range T>
  ossia::domain range_to_domain()
  {
    constexpr auto dom = avnd::get_range<T>();
    return ossia::make_domain(dom.min, dom.max);
  }
  template<avnd::string_parameter T>
  ossia::domain range_to_domain()
  {
    return {};
  }
  template<avnd::bool_parameter T>
  ossia::domain range_to_domain()
  {
    return ossia::domain_base<bool>{};
  }

  template<avnd::enum_parameter T>
  ossia::domain range_to_domain()
  {
    constexpr auto dom = T::choices();
    ossia::domain_base<std::string> d;
    d.values.assign(std::begin(dom), std::end(dom));
    return d;
  }

  template<typename T>
  ossia::domain range_to_domain()
  {
    constexpr auto dom = avnd::get_range<T>();
    if constexpr(requires { std::string_view{dom.values[0].first}; }) {
      // Case for combo_pair things
      using val_type = std::decay_t<decltype(T::value)>;
      ossia::domain_base<val_type> v;
      for(auto& val : dom.values) {
        v.values.push_back(val.second);
      }
      return v;
    }
    else
    {
      // Just strings
      ossia::domain_base<std::string> d;
      d.values.assign(std::begin(dom.values), std::end(dom.values));
      return d;
    }
  }

  template<avnd::int_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::val_type::INT;
    port.domain = this->range_to_domain<Field>();
  }

  template<avnd::float_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::val_type::FLOAT;
    port.domain = this->range_to_domain<Field>();
  }

  template<avnd::bool_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::val_type::BOOL;
    port.domain = this->range_to_domain<Field>();
  }

  template<avnd::string_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::val_type::STRING;
    port.domain = this->range_to_domain<Field>();
  }

  template<avnd::enum_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::val_type::INT;
    port.domain = this->range_to_domain<Field>();
  }

  template<avnd::xy_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::cartesian_2d_u{};
    port.domain = this->range_to_domain<Field>();
  }

  template<avnd::rgb_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::rgba_u{};
    port.domain = {};
  }

  template<typename Field>
  void setup(ossia::value_port& port)
  {
    if constexpr(requires { bool(Field::event); })
      port.is_event = Field::event;
  }
};

template <typename Exec_T>
struct setup_inlets
{
  Exec_T& self;
  ossia::inlets& inlets;

  template<std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::value_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    setup_value_port{}.setup<Field>(*port);

    if constexpr(avnd::has_unit<Field>) {
      constexpr auto unit = avnd::get_unit<Field>();
      if(!unit.empty())
        port->type = ossia::parse_dataspace(unit);
    }
  }

  template<std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::audio_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));
  }

  template<std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::midi_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));
  }

  template<std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::texture_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));
  }
};

template <typename Exec_T>
struct setup_outlets
{
  Exec_T& self;
  ossia::outlets& outlets;
  template<std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::value_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    setup_value_port{}.setup<Field>(*port);

    if constexpr(avnd::has_unit<Field>) {
      constexpr auto unit = avnd::get_unit<Field>();
      if(!unit.empty())
        port->type = ossia::parse_dataspace(unit);
    }
  }
  template<std::size_t Idx, avnd::callback Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::value_outlet& port) const noexcept
  {
    // FIXME likely there are interesting things to do
    outlets.push_back(std::addressof(port));
  }

  template<std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::audio_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));
  }

  template<std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::midi_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));
  }

  template<std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::texture_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));
  }
};

}
