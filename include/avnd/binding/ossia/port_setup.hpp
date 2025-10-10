#pragma once
#include <avnd/binding/ossia/port_base.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/soundfile.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/common/extended_types.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <tuplet/tuple.hpp>

namespace oscr
{
// Compile-time map of avnd concept to ossia port

template <typename N, typename T>
struct get_ossia_inlet_type;
template <typename N, avnd::audio_port T>
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::audio_inlet;
};

template <typename N, avnd::parameter T>
  requires(!ossia_port<T>)
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::value_inlet;
};
template <typename N, avnd::parameter T>
  requires(ossia_value_port<T>)
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::value_inlet;
};
template <typename N, avnd::parameter T>
  requires(ossia_audio_port<T>)
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::audio_inlet;
};
template <typename N, avnd::parameter T>
  requires(ossia_midi_port<T>)
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::midi_inlet;
};

template <typename N, avnd::dynamic_ports_port T>
struct get_ossia_inlet_type<N, T>
{
  using base_type = typename decltype(T{}.ports)::value_type;
  using type = std::vector<typename get_ossia_inlet_type<N, base_type>::type*>;
};
template <typename N, avnd::midi_port T>
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::midi_inlet;
};
template <typename N, avnd::texture_port T>
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::texture_inlet;
};
template <typename N, avnd::soundfile_port T>
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::value_inlet;
};
template <typename N, avnd::midifile_port T>
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::value_inlet;
};
template <typename N, avnd::raw_file_port T>
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::value_inlet;
};
template <typename N, avnd::message T>
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::value_inlet;
};
template <typename N, avnd::unreflectable_message<N> T>
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::value_inlet;
};
template <typename N, avnd::geometry_port T>
struct get_ossia_inlet_type<N, T>
{
  using type = ossia::geometry_inlet;
};

template <typename N, typename T>
struct get_ossia_outlet_type;
template <typename N, avnd::audio_port T>
struct get_ossia_outlet_type<N, T>
{
  using type = ossia::audio_outlet;
};
template <typename N, avnd::parameter T>
  requires(!ossia_port<T>)
struct get_ossia_outlet_type<N, T>
{
  using type = ossia::value_outlet;
};
template <typename N, avnd::parameter T>
  requires(ossia_value_port<T>)
struct get_ossia_outlet_type<N, T>
{
  using type = ossia::value_outlet;
};
template <typename N, avnd::parameter T>
  requires(ossia_audio_port<T>)
struct get_ossia_outlet_type<N, T>
{
  using type = ossia::audio_outlet;
};
template <typename N, avnd::parameter T>
  requires(ossia_midi_port<T>)
struct get_ossia_outlet_type<N, T>
{
  using type = ossia::midi_outlet;
};

template <typename N, avnd::dynamic_ports_port T>
struct get_ossia_outlet_type<N, T>
{
  using base_type = typename decltype(T{}.ports)::value_type;
  using type = std::vector<typename get_ossia_outlet_type<N, base_type>::type*>;
};
template <typename N, avnd::midi_port T>
struct get_ossia_outlet_type<N, T>
{
  using type = ossia::midi_outlet;
};
template <typename N, avnd::texture_port T>
struct get_ossia_outlet_type<N, T>
{
  using type = ossia::texture_outlet;
};
template <typename N, avnd::attachment_port T>
struct get_ossia_outlet_type<N, T>
{
  using type = ossia::texture_outlet;
};
template <typename N, avnd::callback T>
struct get_ossia_outlet_type<N, T>
{
  using type = ossia::value_outlet;
};
template <typename N, avnd::geometry_port T>
struct get_ossia_outlet_type<N, T>
{
  using type = ossia::geometry_outlet;
};

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

  // inputs_getter<a> -> ossia::value_port
  template <typename Port>
  using inputs_getter = typename get_ossia_inlet_type<T, Port>::type;

  // tuple<ossia::audio_inlet, ossia::audio_outlet, ossia::value_inlet>
  using ossia_inputs_tuple = boost::mp11::mp_rename<
      boost::mp11::mp_transform<inputs_getter, inputs_tuple>, tuplet::tuple>;
  ossia_inputs_tuple ports;
};

template <typename T>
struct outlet_storage
{
  using outputs_type = typename avnd::outputs_type<T>::type;
  using outputs_tuple = typename avnd::outputs_type<T>::tuple;

  template <typename Port>
  using outputs_getter = typename get_ossia_outlet_type<T, Port>::type;

  using ossia_outputs_tuple = boost::mp11::mp_rename<
      boost::mp11::mp_transform<outputs_getter, outputs_tuple>, tuplet::tuple>;
  ossia_outputs_tuple ports;
};

struct setup_value_port
{

  static constexpr double min_domain_value = std::numeric_limits<int>::lowest() / 256.;
  static constexpr double max_domain_value = std::numeric_limits<int>::max() / 256.;

  template <avnd::parameter_with_minmax_range_ignore_init T>
  static ossia::domain range_to_domain()
  {
    static constexpr auto dom = avnd::get_range<T>();
    if constexpr(std::is_floating_point_v<decltype(dom.min)>)
      if constexpr(dom.min > min_domain_value && dom.max < max_domain_value)
        return ossia::make_domain((float)dom.min, (float)dom.max);

    if constexpr(avnd::parameter_with_minmax_range<T>)
    {
      if constexpr(std::is_same_v<std::decay_t<decltype(dom.init)>, bool>)
        return ossia::domain_base<bool>{};
      else if constexpr(dom.min > min_domain_value && dom.max < max_domain_value)
        return ossia::make_domain(dom.min, dom.max);
    }
    else if constexpr(dom.min > min_domain_value && dom.max < max_domain_value)
      return ossia::make_domain(dom.min, dom.max);

    return ossia::domain{};
  }

  template <avnd::string_parameter T>
  static ossia::domain range_to_domain()
  {
    // FIXME
    return {};
  }

  template <avnd::enum_parameter T>
  static ossia::domain range_to_domain()
  {
    ossia::domain_base<std::string> d;
#if !defined(_MSC_VER)
    static constexpr auto dom = avnd::get_enum_choices<T>();
    d.values.assign(std::begin(dom), std::end(dom));
#endif
    return d;
  }

  template <typename T>
  static ossia::domain range_to_domain()
  {
    if constexpr(avnd::has_range<T> && requires { avnd::get_range<T>().values; })
    {
      static constexpr auto dom = avnd::get_range<T>();
      if constexpr(requires { std::string_view{dom.values[0].first}; })
      {
        // Case for combo_pair things
        using val_type = std::decay_t<decltype(T::value)>;
        ossia::domain_base<val_type> v;
        for(auto& val : dom.values)
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

  template <typename Field>
  static constexpr void setup_port_is_event(ossia::value_port& port) noexcept
  {
    if constexpr(requires { bool(Field::event); })
      port.is_event = Field::event;
    else
      port.is_event = true;
  }

  template <typename Field>
    requires avnd::optional_ish<decltype(Field::value)>
  static void setup(ossia::value_port& port)
  {
    using concrete_val_type
        = std::remove_cvref_t<decltype(*std::declval<Field>().value)>;
    struct fake_parameter : Field
    {
      concrete_val_type value;
    };
    setup<fake_parameter>(port);
  }

  template <avnd::int_parameter Field>
  static void setup(ossia::value_port& port)
  {
    setup_port_is_event<Field>(port);
    port.type = ossia::val_type::INT;
    port.domain = setup_value_port::range_to_domain<Field>();
  }

  template <avnd::float_parameter Field>
  static void setup(ossia::value_port& port)
  {
    setup_port_is_event<Field>(port);
    port.type = ossia::val_type::FLOAT;
    port.domain = setup_value_port::range_to_domain<Field>();
  }

  template <avnd::bool_parameter Field>
  static void setup(ossia::value_port& port)
  {
    setup_port_is_event<Field>(port);
    port.type = ossia::val_type::BOOL;
    port.domain = setup_value_port::range_to_domain<Field>();
  }

  template <avnd::string_parameter Field>
  static void setup(ossia::value_port& port)
  {
    setup_port_is_event<Field>(port);
    port.type = ossia::val_type::STRING;
    port.domain = setup_value_port::range_to_domain<Field>();
  }

  template <avnd::soundfile_port Field>
  static void setup(ossia::value_port& port)
  {
    setup_port_is_event<Field>(port);
    port.type = ossia::val_type::STRING;
    port.domain = ossia::domain{};
  }

  template <avnd::enum_parameter Field>
  static void setup(ossia::value_port& port)
  {
    setup_port_is_event<Field>(port);
    port.type = ossia::val_type::INT;
    port.domain = setup_value_port::range_to_domain<Field>();
  }

  template <avnd::xy_parameter Field>
  static void setup(ossia::value_port& port)
  {
    setup_port_is_event<Field>(port);
    port.type = ossia::cartesian_2d_u{};
    port.domain = setup_value_port::range_to_domain<Field>();
  }

  template <avnd::rgb_parameter Field>
  static void setup(ossia::value_port& port)
  {
    setup_port_is_event<Field>(port);
    port.type = ossia::rgba_u{};
    port.domain = {};
  }

  template <avnd::span_parameter Field>
  static void setup(ossia::value_port& port)
  {
    setup_port_is_event<Field>(port);
    port.type = ossia::list_type();
    port.domain = setup_value_port::range_to_domain<Field>();
  }

  template <typename Field>
  static void setup(ossia::value_port& port)
  {
    if constexpr(requires { bool(Field::event); })
      port.is_event = Field::event;
    if constexpr(requires { Field::value; })
    {
      if constexpr(avnd::optional_ish<decltype(Field::value)>)
        port.is_event = true;
    }
  }
};

template <typename Exec_T>
struct setup_inlets
{
  Exec_T& self;
  ossia::inlets& inlets;

  template <std::size_t Idx, typename Field>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, ossia::value_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));

    setup_value_port{}.setup<Field>(*port);

    if constexpr(avnd::has_unit<Field>)
    {
      static constexpr auto unit = avnd::get_unit<Field>();
      if(!unit.empty())
        port->type = ossia::parse_dataspace(unit);
    }
  }

  template <std::size_t Idx, typename Field>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl,
      std::vector<ossia::value_inlet*>& port) const noexcept
  {
    int expected = self.dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
    while(port.size() < expected)
      port.push_back(new ossia::value_inlet); // FIXME not freed
    while(port.size() > expected)
      port.erase(port.rbegin().base());

    for(auto& p : port)
      (*this)(ctrl, *p);
  }

  template <std::size_t Idx, typename Field>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, ossia::audio_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));
  }

  template <std::size_t Idx, typename Field>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, ossia::midi_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));
  }

  template <std::size_t Idx, typename Field>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, ossia::texture_inlet& port) const noexcept
  {
    inlets.push_back(std::addressof(port));
  }
  template <std::size_t Idx, avnd::geometry_port Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::geometry_inlet& port)
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
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, ossia::value_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));

    setup_value_port{}.setup<Field>(*port);

    if constexpr(avnd::has_unit<Field>)
    {
      static constexpr auto unit = avnd::get_unit<Field>();
      if(!unit.empty())
        port->type = ossia::parse_dataspace(unit);
    }
  }

  template <std::size_t Idx, typename Field>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl,
      std::vector<ossia::value_outlet*>& port) const noexcept
  {
    const std::size_t expected
        = self.dynamic_ports.num_out_ports(avnd::field_index<Idx>{});
    while(port.size() < expected)
      port.push_back(new ossia::value_outlet); // FIXME not freed
    while(port.size() > expected)
      port.erase(port.rbegin().base());

    for(auto& p : port)
      (*this)(ctrl, *p);
  }

  template <std::size_t Idx, avnd::callback Field>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, ossia::value_outlet& port) const noexcept
  {
    // FIXME likely there are interesting things to do
    outlets.push_back(std::addressof(port));
  }

  template <std::size_t Idx, typename Field>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, ossia::audio_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));
  }

  template <std::size_t Idx, typename Field>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, ossia::midi_outlet& port) const noexcept
  {
    outlets.push_back(std::addressof(port));
  }

  template <std::size_t Idx, typename Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::texture_outlet& port)
      const noexcept
  {
    outlets.push_back(std::addressof(port));
  }

  template <std::size_t Idx, avnd::geometry_port Field>
  void operator()(avnd::field_reflection<Idx, Field> ctrl, ossia::geometry_outlet& port)
      const noexcept
  {
    outlets.push_back(std::addressof(port));
  }
};

template <typename Exec_T, typename Obj_T>
struct setup_variable_audio_ports
{
  Exec_T& self;
  Obj_T& impl;
  int instance{};

  using in_refl = avnd::audio_bus_input_introspection<Obj_T>;
  using out_refl = avnd::audio_bus_output_introspection<Obj_T>;

  template <avnd::variable_poly_audio_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::audio_inlet& port, avnd::field_index<Idx>) const noexcept
  {
    ctrl.request_channels = [&ctrl, &s = self](int x) {
      ctrl.channels = x;
      s.channels.set_input_channels(s.impl, in_refl::template unmap<Idx>(), x);
    };
  }

  template <avnd::variable_poly_audio_port Field, std::size_t Idx>
  void operator()(
      Field& ctrl, ossia::audio_outlet& port, avnd::field_index<Idx>) const noexcept
  {
    ctrl.request_channels = [&ctrl, &s = self](int x) {
      ctrl.channels = x;
      s.channels.set_output_channels(s.impl, out_refl::template unmap<Idx>(), x);
    };
  }

  void operator()(auto&&...) const noexcept { }
};

template <typename Exec_T, typename Obj_T>
struct setup_raw_ossia_ports
{
  Exec_T& self;
  Obj_T& impl;
  int instance{};

  template <ossia_port Field, std::size_t Idx>
  void operator()(Field& ctrl, auto& port, avnd::field_index<Idx>) const noexcept
  {
    ctrl.value = &port.data;

    if constexpr(Field::is_event())
    {
      if_possible(ctrl.value->is_event = true);
    }
  }

  void operator()(auto&&...) const noexcept { }
};
}
