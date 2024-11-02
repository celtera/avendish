#pragma once
#include <avnd/binding/ossia/to_value.hpp>
#include <ossia/dataflow/value_port.hpp>
#include <tuplet/tuple.hpp>

namespace oscr
{

template <typename Self, std::size_t Idx, typename Field, typename R, typename... Args>
struct do_callback;

template <typename Self, std::size_t Idx, typename Field, typename R, typename... Args>
  requires(!avnd::tag_timestamp<Field>)
struct do_callback<Self, Idx, Field, R, Args...>
{
  Self& self;
  Field& field;
  R operator()(Args... args)
  {
    // Idx is the index of the port in the complete input array.
    // We need to map it to the callback index.
    ossia::value_outlet& port = tuplet::get<Idx>(self.ossia_outlets.ports);
    if constexpr(sizeof...(Args) == 0)
    {
      port.data.write_value(ossia::impulse{}, self.start_frame_for_this_tick);
    }
    else if constexpr(sizeof...(Args) == 1)
    {
      port.data.write_value(
          to_ossia_value(field, args...), self.start_frame_for_this_tick);
    }
    else
    {
      std::vector<ossia::value> vec{to_ossia_value(field, args)...};
      port.data.write_value(std::move(vec), self.start_frame_for_this_tick);
    }

    // FIXME handle control feedback !
    // if constexpr(avnd::control<Field>)
    // {
    //   // Get the index of the control in [0; N[
    //   using type = typename Exec_T::processor_type;
    //   using controls = avnd::control_output_introspection<type>;
    //   constexpr int control_index = controls::field_index_to_index(idx);
    //
    //   // Mark the control as changed
    //   self.control.outputs_set.set(control_index);
    // }
    if constexpr(!std::is_void_v<R>)
      return R{};
  }
};

template <typename Self, std::size_t Idx, typename Field, typename R, typename... Args>
  requires(avnd::tag_timestamp<Field>)
struct do_callback<Self, Idx, Field, R, Args...>
{
  Self& self;
  Field& field;

  void do_call(int64_t ts)
  {
    // Idx is the index of the port in the complete input array.
    // We need to map it to the callback index.
    ossia::value_outlet& port = tuplet::get<Idx>(self.ossia_outlets.ports);
    port.data.write_value(ossia::impulse{}, self.start_frame_for_this_tick + ts);
  }

  template <typename U>
  void do_call(int64_t ts, U&& u)
  {
    // Idx is the index of the port in the complete input array.
    // We need to map it to the callback index.
    ossia::value_outlet& port = tuplet::get<Idx>(self.ossia_outlets.ports);
    port.data.write_value(
        to_ossia_value(field, std::forward<U>(u)), self.start_frame_for_this_tick + ts);
  }

  template <typename... U>
  void do_call(int64_t ts, U&&... us)
  {
    ossia::value_outlet& port = tuplet::get<Idx>(self.ossia_outlets.ports);
    std::vector<ossia::value> vec{to_ossia_value(field, us)...};
    port.data.write_value(std::move(vec), self.start_frame_for_this_tick + ts);
  }

  R operator()(Args... args)
  {
    do_call(static_cast<Args&&>(args)...);
    if constexpr(!std::is_void_v<R>)
      return R{};
  }
};
}
