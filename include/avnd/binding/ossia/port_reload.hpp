#pragma once
#include <avnd/binding/ossia/dynamic_ports.hpp>
#include <avnd/binding/ossia/port_setup.hpp>

#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

namespace oscr
{
// Live reloading of the ports of a node whose dynamic port count changed.
//
// The exec node keeps its ossia ports as members (regular ports) or as vectors of
// heap-allocated ports (dynamic ports). Replacing the dynamic ones has to be split
// in two phases so that the execution thread never allocates or frees:
//
//  1. reload_inlets / reload_outlets, in the main thread: for every regular port,
//     record its address; for every dynamic port, build a brand new vector of ports
//     of the requested size. Both go in an {inlet,outlet}_reload_storage, which
//     owns the new dynamic ports until they are handed over.
//  2. apply_inlets / apply_outlets, in the execution thread: swap the new vectors
//     into the node. Afterwards the storage owns the previous ports, and it must be
//     destroyed in the main thread so that they are freed there.
//
// If phase 2 never runs (the node went away first), the storage still owns the
// unused new ports and frees them.

// Regular port: pointer to the port living in the node.
// Dynamic port: a vector of ports, owned.
template <typename T>
struct reload_slot_type
{
  using type = T*;
};
template <typename T>
struct reload_slot_type<std::vector<T*>>
{
  using type = std::vector<T*>;
};
template <typename T>
using reload_slot_t = typename reload_slot_type<T>::type;

template <typename T>
struct inlet_reload_storage
{
  // typelist<a, b, c>
  using inputs_tuple = typename avnd::inputs_type<T>::tuple;

  // inputs_getter<a> -> ossia::value_inlet* or std::vector<ossia::value_inlet*>
  template <typename Port>
  using inputs_getter = reload_slot_t<typename get_ossia_inlet_type<T, Port>::type>;

  using ossia_inputs_tuple = boost::mp11::mp_rename<
      boost::mp11::mp_transform<inputs_getter, inputs_tuple>, tuplet::tuple>;
  ossia_inputs_tuple ports{};

  inlet_reload_storage() = default;
  inlet_reload_storage(const inlet_reload_storage&) = delete;
  inlet_reload_storage& operator=(const inlet_reload_storage&) = delete;
  inlet_reload_storage(inlet_reload_storage&&) = delete;
  inlet_reload_storage& operator=(inlet_reload_storage&&) = delete;
  ~inlet_reload_storage() { delete_dynamic_ports(ports); }
};

template <typename T>
struct outlet_reload_storage
{
  using outputs_tuple = typename avnd::outputs_type<T>::tuple;

  template <typename Port>
  using outputs_getter = reload_slot_t<typename get_ossia_outlet_type<T, Port>::type>;

  using ossia_outputs_tuple = boost::mp11::mp_rename<
      boost::mp11::mp_transform<outputs_getter, outputs_tuple>, tuplet::tuple>;
  ossia_outputs_tuple ports{};

  outlet_reload_storage() = default;
  outlet_reload_storage(const outlet_reload_storage&) = delete;
  outlet_reload_storage& operator=(const outlet_reload_storage&) = delete;
  outlet_reload_storage(outlet_reload_storage&&) = delete;
  outlet_reload_storage& operator=(outlet_reload_storage&&) = delete;
  ~outlet_reload_storage() { delete_dynamic_ports(ports); }
};

// Phase 1 (main thread)
template <typename Exec_T>
struct reload_inlets
{
  Exec_T& self;
  ossia::inlets& inlets;
  oscr::dynamic_ports_storage<typename Exec_T::processor_type>& dynamic_ports;

  // Regular port: it stays where it is, in the node, and keeps the type,
  // domain and unit it was given when the node was created. Only its address
  // is taken: the exec thread may be reading it right now (the value domain
  // owns memory), so nothing is written to it from here.
  template <std::size_t Idx, typename Field, typename OssiaPortType>
  void operator()(
      avnd::field_reflection<Idx, Field>, OssiaPortType& port,
      OssiaPortType*& pout) const noexcept
  {
    inlets.push_back(std::addressof(port));
    pout = &port;
  }

  // Dynamic port: a fresh set of ports, handed over in the exec thread
  template <std::size_t Idx, typename Field, typename OssiaPortType>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, std::vector<OssiaPortType*>&,
      std::vector<OssiaPortType*>& out) const noexcept
  {
    const int expected = std::max(0, dynamic_ports.num_in_ports(avnd::field_index<Idx>{}));

    for(auto p : out)
      delete p;
    out.clear();
    out.reserve(expected);
    for(int i = 0; i < expected; i++)
      out.push_back(new OssiaPortType);

    for(auto p : out)
      setup_inlets<Exec_T>{self, inlets}(ctrl, *p);
  }
};

// Phase 2 (exec thread)
template <typename Exec_T>
struct apply_inlets
{
  Exec_T& self;
  ossia::inlets& inlets;

  template <std::size_t Idx, typename Field, typename OssiaPortType>
  void operator()(
      avnd::field_reflection<Idx, Field>, OssiaPortType&, OssiaPortType*&) const noexcept
  {
  }

  template <std::size_t Idx, typename Field, typename OssiaPortType>
  void operator()(
      avnd::field_reflection<Idx, Field>, std::vector<OssiaPortType*>& port,
      std::vector<OssiaPortType*>& out) const noexcept
  {
    std::swap(port, out);
  }
};

template <typename Exec_T>
struct reload_outlets
{
  Exec_T& self;
  ossia::outlets& outlets;
  oscr::dynamic_ports_storage<typename Exec_T::processor_type>& dynamic_ports;

  // Regular port: see reload_inlets
  template <std::size_t Idx, typename Field, typename OssiaPortType>
  void operator()(
      avnd::field_reflection<Idx, Field>, OssiaPortType& port,
      OssiaPortType*& pout) const noexcept
  {
    outlets.push_back(std::addressof(port));
    pout = &port;
  }

  template <std::size_t Idx, typename Field, typename OssiaPortType>
  void operator()(
      avnd::field_reflection<Idx, Field> ctrl, std::vector<OssiaPortType*>&,
      std::vector<OssiaPortType*>& out) const noexcept
  {
    const int expected
        = std::max(0, dynamic_ports.num_out_ports(avnd::field_index<Idx>{}));

    for(auto p : out)
      delete p;
    out.clear();
    out.reserve(expected);
    for(int i = 0; i < expected; i++)
      out.push_back(new OssiaPortType);

    for(auto p : out)
      setup_outlets<Exec_T>{self, outlets}(ctrl, *p);
  }
};

template <typename Exec_T>
struct apply_outlets
{
  Exec_T& self;
  ossia::outlets& outlets;

  template <std::size_t Idx, typename Field, typename OssiaPortType>
  void operator()(
      avnd::field_reflection<Idx, Field>, OssiaPortType&, OssiaPortType*&) const noexcept
  {
  }

  template <std::size_t Idx, typename Field, typename OssiaPortType>
  void operator()(
      avnd::field_reflection<Idx, Field>, std::vector<OssiaPortType*>& port,
      std::vector<OssiaPortType*>& out) const noexcept
  {
    std::swap(port, out);
  }
};
}
