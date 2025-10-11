#pragma once

#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/dynamic_ports.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>

namespace oscr
{
template <typename T>
concept has_dynamic_ports = avnd::dynamic_ports_input_introspection<T>::size > 0
                            || avnd::dynamic_ports_output_introspection<T>::size > 0;

template <typename Field>
struct dynamic_ports_state_type;

template <avnd::dynamic_ports_port Field>
struct dynamic_ports_state_type<Field>
{
  int count = 0;
};

template <typename T>
struct dynamic_ports_storage
{
  template <std::size_t Idx>
  int num_in_ports(avnd::field_index<Idx>) const noexcept
  {
    return 1;
  }
  template <std::size_t Idx>
  int num_out_ports(avnd::field_index<Idx>) const noexcept
  {
    return 1;
  }
};

template <typename T>
  requires(
      avnd::dynamic_ports_input_introspection<T>::size > 0
      || avnd::dynamic_ports_output_introspection<T>::size > 0)
struct dynamic_ports_storage<T>
{
  using in_tuple = avnd::filter_and_apply<
      dynamic_ports_state_type, avnd::dynamic_ports_input_introspection, T>;
  using out_tuple = avnd::filter_and_apply<
      dynamic_ports_state_type, avnd::dynamic_ports_output_introspection, T>;

  AVND_NO_UNIQUE_ADDRESS in_tuple in_handles;
  AVND_NO_UNIQUE_ADDRESS out_tuple out_handles;

  template <std::size_t Idx>
  int num_in_ports(avnd::field_index<Idx> f) const noexcept
  {
    static constexpr std::size_t pred_idx
        = avnd::dynamic_ports_input_introspection<T>::field_index_to_index(f);
    return std::get<pred_idx>(in_handles).count;
  }
  template <std::size_t Idx>
  int num_out_ports(avnd::field_index<Idx> f) const noexcept
  {
    static constexpr std::size_t pred_idx
        = avnd::dynamic_ports_output_introspection<T>::field_index_to_index(f);
    return std::get<pred_idx>(out_handles).count;
  }
  template <std::size_t Idx>
  int& num_in_ports(avnd::field_index<Idx> f) noexcept
  {
    static constexpr std::size_t pred_idx
        = avnd::dynamic_ports_input_introspection<T>::field_index_to_index(f);
    return std::get<pred_idx>(in_handles).count;
  }
  template <std::size_t Idx>
  int& num_out_ports(avnd::field_index<Idx> f) noexcept
  {
    static constexpr std::size_t pred_idx
        = avnd::dynamic_ports_output_introspection<T>::field_index_to_index(f);
    return std::get<pred_idx>(out_handles).count;
  }
};

}
