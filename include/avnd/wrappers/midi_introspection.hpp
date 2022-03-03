#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/concepts.hpp>
#include <avnd/wrappers/input_introspection.hpp>
#include <avnd/wrappers/output_introspection.hpp>
#include <avnd/wrappers/port_introspection.hpp>
#include <boost/mp11.hpp>

namespace avnd
{

template <raw_container_midi_port Field>
using midi_message_type
    = std::remove_pointer_t<std::remove_reference_t<decltype(Field::midi_messages)>>;

/**
 * Stores midi input buffers when there are raw ports being used.
 */
template <typename T>
struct midi_input_storage
{
};

/**
 * Stores midi output buffers when there are raw ports being used.
 */
template <typename T>
struct midi_output_storage
{
};

template <typename T>
requires (avnd::raw_container_midi_input_introspection<T>::size > 0)
struct midi_input_storage<T>
{
  using midi_in_messages_tuple
      = filter_and_apply<midi_message_type, raw_container_midi_input_introspection, T>;
  using midi_in_messages_vectors
      = boost::mp11::mp_transform<std::vector, midi_in_messages_tuple>;

  [[no_unique_address]]
  midi_in_messages_vectors inputs_storage;
};

template <typename T>
requires (avnd::raw_container_midi_output_introspection<T>::size > 0)
struct midi_output_storage<T>
{
  using midi_out_messages_tuple
  = filter_and_apply<midi_message_type, raw_container_midi_output_introspection, T>;
  using midi_out_messages_vectors
  = boost::mp11::mp_transform<std::vector, midi_out_messages_tuple>;

  [[no_unique_address]]
  midi_out_messages_vectors outputs_storage;
};

template <typename T>
struct midi_storage
     : midi_input_storage<T>
     , midi_output_storage<T>
{
  using midi_in_info = avnd::midi_input_introspection<T>;
  using midi_out_info = avnd::midi_output_introspection<T>;
  using raw_midi_in_info = avnd::raw_container_midi_input_introspection<T>;
  using raw_midi_out_info = avnd::raw_container_midi_output_introspection<T>;
  using dyn_midi_in_info = avnd::dynamic_container_midi_input_introspection<T>;
  using dyn_midi_out_info = avnd::dynamic_container_midi_output_introspection<T>;

  void reserve_space(avnd::effect_container<T>& t, int buffer_size)
  {
    if constexpr(raw_midi_in_info::size > 0)
    {
      auto init_raw_in = [&]<auto Idx, typename M>(M& port, avnd::num<Idx>) {
        // Here we use storage pre-allocated in midi_..._storage
        // We allocate some memory locally and save a pointer in the structure.
        auto& buf = std::get<Idx>(this->inputs_storage);
        buf.resize(buffer_size);

        port.midi_messages = buf.data();
        port.size = 0;
      };
      raw_midi_in_info::for_all_n(avnd::get_inputs(t), init_raw_in);
    }

    if constexpr(raw_midi_out_info::size > 0)
    {
      auto init_raw_out = [&]<auto Idx, typename M>(M& port, avnd::num<Idx>) {
        // Here we use storage pre-allocated in midi_..._storage
        // We allocate some memory locally and save a pointer in the structure.
        auto& buf = std::get<Idx>(this->outputs_storage);
        buf.resize(buffer_size);

        port.midi_messages = buf.data();
        port.size = 0;
      };
      raw_midi_out_info::for_all_n(avnd::get_outputs(t), init_raw_out);
    }

    auto init_dyn = [&](auto& port) {
      // Here we use the vector in the port directly.
      port.midi_messages.clear();
      port.midi_messages.reserve(buffer_size);
    };
    dyn_midi_in_info::for_all(avnd::get_inputs(t),init_dyn);
    dyn_midi_out_info::for_all(avnd::get_outputs(t),init_dyn);
  }

  void do_clear(avnd::dynamic_container_midi_port auto& port)
  {
    port.midi_messages.clear();
  }

  void do_clear(avnd::raw_container_midi_port auto& port)
  {
    port.size = 0;
  }

  void clear_inputs(avnd::effect_container<T>& t)
  {
    if constexpr(midi_in_info::size > 0)
    {
      auto clearer = [this] (auto&& port) {
        this->do_clear(port);
      };

      midi_in_info::for_all(avnd::get_inputs(t), clearer);
    }
  }

  void clear_outputs(avnd::effect_container<T>& t)
  {
    if constexpr(midi_in_info::size > 0)
    {
      auto clearer = [this] (auto&& port) {
        this->do_clear(port);
      };

      midi_out_info::for_all(avnd::get_outputs(t), clearer);
    }
  }
};
}
