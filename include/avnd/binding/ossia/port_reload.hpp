#pragma once
#include <avnd/binding/ossia/dynamic_ports.hpp>
#include <avnd/binding/ossia/port_setup.hpp>
namespace oscr
{
template <typename T>
struct inlet_reload_storage
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
      boost::mp11::mp_transform<
          std::add_pointer_t, boost::mp11::mp_transform<inputs_getter, inputs_tuple>>,
      tuplet::tuple>;
  ossia_inputs_tuple ports;
};

template <typename T>
struct outlet_reload_storage
{
  using outputs_type = typename avnd::outputs_type<T>::type;
  using outputs_tuple = typename avnd::outputs_type<T>::tuple;

  template <typename Port>
  using outputs_getter = typename get_ossia_outlet_type<T, Port>::type;

  using ossia_outputs_tuple = boost::mp11::mp_rename<
      boost::mp11::mp_transform<
          std::add_pointer_t, boost::mp11::mp_transform<outputs_getter, outputs_tuple>>,
      tuplet::tuple>;
  ossia_outputs_tuple ports;
};

template <typename Exec_T>
struct reload_inlets
{
  Exec_T& self;
  ossia::inlets& inlets;
  oscr::dynamic_ports_storage<typename Exec_T::processor_type>& dynamic_ports;

  void operator()(auto ctrl, auto& port, auto*& pout) const noexcept
  {
    setup_inlets{self, inlets}(ctrl, port);
    pout = &port;
  }

  template <std::size_t Idx, typename Field, typename OssiaPortType>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, std::vector<OssiaPortType*>&,
      std::vector<OssiaPortType*>*& pout) const noexcept
  {
    int expected = dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
    pout = new std::vector<OssiaPortType*>;
    auto& port = *pout; //FIXME
    while(port.size() < expected)
      port.push_back(new OssiaPortType); // FIXME not freed

    for(auto& p : port)
      setup_inlets{self, inlets}(ctrl, *p);
  }
};

template <typename Exec_T>
struct apply_inlets
{
  Exec_T& self;
  ossia::inlets& inlets;

  void operator()(auto ctrl, auto& port, auto*& pout) const noexcept { }

  template <std::size_t Idx, typename Field, typename OssiaPortType>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, std::vector<OssiaPortType*>& port,
      std::vector<OssiaPortType*>*& pout) const noexcept
  {
    std::swap(port, *pout);
  }
};

template <typename Exec_T>
struct reload_outlets
{
  Exec_T& self;
  ossia::outlets& outlets;
  oscr::dynamic_ports_storage<typename Exec_T::processor_type>& dynamic_ports;

  void operator()(auto ctrl, auto& port, auto*& pout) const noexcept
  {
    setup_outlets{self, outlets}(ctrl, port);
    pout = &port;
  }

  template <std::size_t Idx, typename Field, typename OssiaPortType>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, std::vector<OssiaPortType*>&,
      std::vector<OssiaPortType*>*& pout) const noexcept
  {
    const std::size_t expected = dynamic_ports.num_out_ports(avnd::field_index<Idx>{});
    pout = new std::vector<OssiaPortType*>;
    auto& port = *pout; //FIXME
    while(port.size() < expected)
      port.push_back(new OssiaPortType); // FIXME not freed

    for(auto& p : port)
      setup_outlets{self, outlets}(ctrl, *p);
  }
};

template <typename Exec_T>
struct apply_outlets
{
  Exec_T& self;
  ossia::outlets& outlets;

  void operator()(auto ctrl, auto& port, auto*& pout) const noexcept { }

  template <std::size_t Idx, typename Field, typename OssiaPortType>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, std::vector<OssiaPortType*>& port,
      std::vector<OssiaPortType*>*& pout) const noexcept
  {
    std::swap(port, *pout);
  }
};
}
