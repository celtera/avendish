#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/all.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/port.hpp>
#include <boost/mp11.hpp>
namespace avnd
{
// Field:
//   struct { std::span<T> value; }
// This gives us: std::vector<T>
template <typename Field>
using span_param_values_type = typename decltype(Field::value)::value_type;

// Field: struct { std::optional<float>* values; }
// This gives us: std::optional<float>
template <typename Field>
using linear_timed_param_values_type
    = std::remove_pointer_t<std::remove_reference_t<decltype(Field::values)>>;

// Assuming:
//   struct timed_values { int frames;  float value; };
// Field:
//   struct { std::span<timed_values> values; }
// This gives us: timed_values
template <typename Field>
using span_timed_param_values_type = typename decltype(Field::values)::value_type;

template <typename T>
struct span_param_input_storage
{
};

template <typename T>
  requires(span_parameter_input_introspection<T>::size > 0)
struct span_param_input_storage<T>
{
  // std::tuple< float, int >
  using tuple
      = filter_and_apply<span_param_values_type, span_parameter_input_introspection, T>;

  // std::tuple< std::vector<float>, std::vector<int> >
  using vectors = boost::mp11::mp_transform<std::vector, tuple>;

  AVND_NO_UNIQUE_ADDRESS vectors span_inputs;
};

template <typename T>
struct linear_timed_param_input_storage
{
};

template <typename T>
  requires(linear_timed_parameter_input_introspection<T>::size > 0)
struct linear_timed_param_input_storage<T>
{
  // std::tuple< std::optional<float>, my_optional<int> >
  using tuple = filter_and_apply<
      linear_timed_param_values_type, linear_timed_parameter_input_introspection, T>;

  // std::tuple< std::vector<std::optional<float>>, std::vector<my_optional<int>> >
  using vectors = boost::mp11::mp_transform<std::vector, tuple>;

  AVND_NO_UNIQUE_ADDRESS vectors linear_inputs;
};

template <typename T>
struct span_timed_param_input_storage
{
};

template <typename T>
  requires(span_timed_parameter_input_introspection<T>::size > 0)
struct span_timed_param_input_storage<T>
{
  // std::tuple< std::optional<float>, my_optional<int> >
  using tuple = filter_and_apply<
      span_timed_param_values_type, span_timed_parameter_input_introspection, T>;

  // std::tuple< std::vector<std::optional<float>>, std::vector<my_optional<int>> >
  using vectors = boost::mp11::mp_transform<std::vector, tuple>;

  AVND_NO_UNIQUE_ADDRESS vectors span_timed_inputs;
};

template <typename T>
struct linear_timed_param_output_storage
{
};

template <typename T>
  requires(linear_timed_parameter_output_introspection<T>::size > 0)
struct linear_timed_param_output_storage<T>
{
  // std::tuple< std::optional<float>, my_optional<int> >
  using tuple = filter_and_apply<
      linear_timed_param_values_type, linear_timed_parameter_output_introspection, T>;

  // std::tuple< std::vector<std::optional<float>>, std::vector<my_optional<int>> >
  using vectors = boost::mp11::mp_transform<std::vector, tuple>;

  AVND_NO_UNIQUE_ADDRESS vectors linear_outputs;
};

template <typename T>
struct span_timed_param_output_storage
{
};

template <typename T>
  requires(span_timed_parameter_output_introspection<T>::size > 0)
struct span_timed_param_output_storage<T>
{
  // std::tuple< std::optional<float>, my_optional<int> >
  using tuple = filter_and_apply<
      span_timed_param_values_type, span_timed_parameter_output_introspection, T>;

  // std::tuple< std::vector<std::optional<float>>, std::vector<my_optional<int>> >
  using vectors = boost::mp11::mp_transform<std::vector, tuple>;

  AVND_NO_UNIQUE_ADDRESS vectors span_outputs;
};

/**
 * Used to store buffers for sample-accurate parameters.
 * These associate timestamps with value changes.
 */
template <typename T>
struct control_storage
    : span_param_input_storage<T>
    , linear_timed_param_input_storage<T>
    , linear_timed_param_output_storage<T>
    , span_timed_param_input_storage<T>
    , span_timed_param_output_storage<T>
{
  using span_in = span_parameter_input_introspection<T>;
  using lin_in = linear_timed_parameter_input_introspection<T>;
  using span_timed_in = span_timed_parameter_input_introspection<T>;
  using dyn_in = dynamic_timed_parameter_input_introspection<T>;
  using lin_out = linear_timed_parameter_output_introspection<T>;
  using span_out = span_timed_parameter_output_introspection<T>;
  using dyn_out = dynamic_timed_parameter_output_introspection<T>;

  void reserve_space(avnd::effect_container<T>& t, int buffer_size)
  {
    if constexpr(lin_in::size > 0)
    {
      auto init_raw_in = [&]<auto Idx, typename M>(M & port, avnd::predicate_index<Idx>)
      {
        // Get the matching buffer in our storage
        auto& buf = tpl::get<Idx>(this->linear_inputs);

        // Allocate enough space for the new buffer size
        buf.resize(buffer_size);

        // Assign the pointer to the std::optional<float*> values; member in the port
        port.values = buf.data();
      };
      lin_in::for_all_n(avnd::get_inputs(t), init_raw_in);
    }
    if constexpr(lin_out::size > 0)
    {
      auto init_raw_out = [&]<auto Idx, typename M>(M & port, avnd::predicate_index<Idx>)
      {
        auto& buf = tpl::get<Idx>(this->linear_outputs);
        buf.resize(buffer_size);
        port.values = buf.data();
      };
      lin_out::for_all_n(avnd::get_outputs(t), init_raw_out);
    }

    if constexpr(span_timed_in::size > 0)
    {
      auto init_raw_in = [&]<auto Idx, typename M>(M & port, avnd::predicate_index<Idx>)
      {
        // Get the matching buffer in our storage, a std::vector<timed_value>
        auto& buf = tpl::get<Idx>(this->span_timed_inputs);

        // Allocate enough space for the new buffer size
        buf.reserve(buffer_size);

        // Assign the pointer to the std::span<timed_value> values; member in the port
        port.values = {buf.data(), std::size_t(0)};
      };
      span_timed_in::for_all_n(avnd::get_inputs(t), init_raw_in);
    }
    if constexpr(span_out::size > 0)
    {
      auto init_raw_out = [&]<auto Idx, typename M>(M & port, avnd::predicate_index<Idx>)
      {
        auto& buf = tpl::get<Idx>(this->span_outputs);
        buf.reserve(buffer_size);
        port.values = {buf.data(), std::size_t(0)};
      };
      span_out::for_all_n(avnd::get_outputs(t), init_raw_out);
    }

    auto init_dyn = [&](auto& port) {
      // Here we use the vector in the port directly.
      port.values.clear();
      if_possible(port.values.reserve(buffer_size));
    };
    dyn_in::for_all(avnd::get_inputs(t), init_dyn);
    dyn_out::for_all(avnd::get_outputs(t), init_dyn);
  }

  void clear_inputs(avnd::effect_container<T>& t)
  {
    if constexpr(span_in::size > 0)
    {
      auto init_raw_in = [&]<auto Idx, typename M>(M & port, avnd::predicate_index<Idx>)
      {
        auto& buf = tpl::get<Idx>(this->span_inputs);
        buf.resize(0);
      };
      span_in::for_all_n(avnd::get_inputs(t), init_raw_in);
    }

    if constexpr(lin_in::size > 0)
    {
      auto clear_raw_in = [&]<auto Idx, typename M>(M & port, avnd::predicate_index<Idx>)
      {
        auto& buf = tpl::get<Idx>(this->linear_inputs);
        for(auto& b : buf)
          b = {};
      };
      lin_in::for_all_n(avnd::get_inputs(t), clear_raw_in);
    }

    if constexpr(span_timed_in::size > 0)
    {
      auto init_raw_in = [&]<auto Idx, typename M>(M & port, avnd::predicate_index<Idx>)
      {
        auto& buf = tpl::get<Idx>(this->span_timed_inputs);
        buf.resize(0);
      };
      span_timed_in::for_all_n(avnd::get_inputs(t), init_raw_in);
    }

    auto init_dyn = [&](auto&& port) { port.values.clear(); };
    dyn_in::for_all(avnd::get_inputs(t), init_dyn);
  }

  void clear_outputs(avnd::effect_container<T>& t)
  {
    if constexpr(lin_out::size > 0)
    {
      auto clear_raw_out
          = [&]<auto Idx, typename M>(M & port, avnd::predicate_index<Idx>)
      {
        auto& buf = tpl::get<Idx>(this->linear_outputs);
        for(auto& b : buf)
          b = {};
      };
      lin_out::for_all_n(avnd::get_outputs(t), clear_raw_out);
    }

    if constexpr(span_out::size > 0)
    {
      auto init_raw_out = [&]<auto Idx, typename M>(M & port, avnd::predicate_index<Idx>)
      {
        auto& buf = tpl::get<Idx>(this->span_outputs);
        buf.resize(0);
      };
      span_out::for_all_n(avnd::get_outputs(t), init_raw_out);
    }

    auto init_dyn = [&](auto& port) { port.values.clear(); };
    dyn_out::for_all(avnd::get_outputs(t), init_dyn);
  }
};

/**
 * Used to store buffers for CV parameters.
 * These are like audio arrays, but for any type
 */
template <typename T>
struct cv_storage
{
  void reserve_space(avnd::effect_container<T>& t, int buffer_size) { }
};
}
