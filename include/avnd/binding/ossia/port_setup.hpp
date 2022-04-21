#pragma once
#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/soundfile.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

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
template <avnd::soundfile_port T>
struct get_ossia_inlet_type<T>
{
    using type = ossia::value_inlet;
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
template <avnd::attachment_port T>
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

  // typelist<a, b, c>
  using inputs_tuple = typename avnd::inputs_type<T>::tuple;

  // tuple<ossia::audio_inlet, ossia::audio_outlet, ossia::value_inlet>
  using ossia_inputs_tuple
      = boost::mp11::mp_rename<
          boost::mp11::mp_transform<get_ossia_inlet_type_t, inputs_tuple>,
        tuplet::tuple>;
  ossia_inputs_tuple ports;
};

template <typename T>
struct outlet_storage
{
  using outputs_type = typename avnd::outputs_type<T>::type;
  using outputs_tuple = typename avnd::outputs_type<T>::tuple;

  using ossia_outputs_tuple
      = boost::mp11::mp_rename<
          boost::mp11::mp_transform<get_ossia_outlet_type_t, outputs_tuple>,
        tuplet::tuple>;
  ossia_outputs_tuple ports;
};

struct setup_value_port
{
  template <avnd::parameter_with_minmax_range T>
  ossia::domain range_to_domain()
  {
    constexpr auto dom = avnd::get_range<T>();
    return ossia::make_domain(dom.min, dom.max);
  }
  template <avnd::string_parameter T>
  ossia::domain range_to_domain()
  {
    return {};
  }
  template <avnd::bool_parameter T>
  ossia::domain range_to_domain()
  {
    return ossia::domain_base<bool>{};
  }

  template <avnd::enum_parameter T>
  ossia::domain range_to_domain()
  {
    constexpr auto dom = avnd::get_enum_choices<T>();
    ossia::domain_base<std::string> d;
    d.values.assign(std::begin(dom), std::end(dom));
    return d;
  }

  template <typename T>
  ossia::domain range_to_domain()
  {
    if constexpr(avnd::has_range<T>)
    {
      constexpr auto dom = avnd::get_range<T>();
      if constexpr (requires { std::string_view{dom.values[0].first}; })
      {
        // Case for combo_pair things
        using val_type = std::decay_t<decltype(T::value)>;
        ossia::domain_base<val_type> v;
        for (auto& val : dom.values)
        {
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
    else
    {
      return {};
    }
  }

  template <avnd::int_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::val_type::INT;
    port.domain = this->range_to_domain<Field>();
  }

  template <avnd::float_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::val_type::FLOAT;
    port.domain = this->range_to_domain<Field>();
  }

  template <avnd::bool_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::val_type::BOOL;
    port.domain = this->range_to_domain<Field>();
  }

  template <avnd::string_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::val_type::STRING;
    port.domain = this->range_to_domain<Field>();
  }

  template <avnd::soundfile_port Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::val_type::STRING;
    port.domain = ossia::domain{};
  }

  template <avnd::enum_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::val_type::INT;
    port.domain = this->range_to_domain<Field>();
  }

  template <avnd::xy_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::cartesian_2d_u{};
    port.domain = this->range_to_domain<Field>();
  }

  template <avnd::rgb_parameter Field>
  void setup(ossia::value_port& port)
  {
    port.is_event = true;
    port.type = ossia::rgba_u{};
    port.domain = {};
  }

  template <typename Field>
  void setup(ossia::value_port& port)
  {
    if constexpr (requires { bool(Field::event); })
      port.is_event = Field::event;
  }
};

template <typename Exec_T>
struct setup_inlets
{
  Exec_T& self;
  ossia::inlets& inlets;

  template <std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::value_inlet& port)
      const noexcept
  {
    inlets.push_back(std::addressof(port));

    setup_value_port{}.setup<Field>(*port);

    if constexpr (avnd::has_unit<Field>)
    {
      constexpr auto unit = avnd::get_unit<Field>();
      if (!unit.empty())
        port->type = ossia::parse_dataspace(unit);
    }
  }

  template <std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::audio_inlet& port)
      const noexcept
  {
    inlets.push_back(std::addressof(port));
  }

  template <std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::midi_inlet& port)
      const noexcept
  {
    inlets.push_back(std::addressof(port));
  }

  template <std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::texture_inlet& port)
      const noexcept
  {
    inlets.push_back(std::addressof(port));
  }
};

template <typename Exec_T>
struct setup_outlets
{
  Exec_T& self;
  ossia::outlets& outlets;
  template <std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::value_outlet& port)
      const noexcept
  {
    outlets.push_back(std::addressof(port));

    setup_value_port{}.setup<Field>(*port);

    if constexpr (avnd::has_unit<Field>)
    {
      constexpr auto unit = avnd::get_unit<Field>();
      if (!unit.empty())
        port->type = ossia::parse_dataspace(unit);
    }
  }
  template <std::size_t Idx, avnd::callback Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::value_outlet& port)
      const noexcept
  {
    // FIXME likely there are interesting things to do
    outlets.push_back(std::addressof(port));
  }

  template <std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::audio_outlet& port)
      const noexcept
  {
    outlets.push_back(std::addressof(port));
  }

  template <std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::midi_outlet& port)
      const noexcept
  {
    outlets.push_back(std::addressof(port));
  }

  template <std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::texture_outlet& port)
      const noexcept
  {
    outlets.push_back(std::addressof(port));
  }
};

}
