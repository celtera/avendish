#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <avnd/concepts/all.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/mp11.hpp>
namespace avnd
{
// Field: struct { float smoothing_ratio(); T value; }
// This gives us: { T current_value, T target_value; }
template <typename Field>
using smooth_param_value_type = std::remove_reference_t<decltype(Field::value)>;

template <typename Field>
struct smooth_param_storage_type
{
  smooth_param_value_type<Field> current_value;
  smooth_param_value_type<Field> target_value;
  double smooth_rate{}; // exp(-2pi / (0.0001 * smooth * fs))
};

template <typename T>
struct smooth_param_storage
{
};

template <typename T>
  requires(smooth_parameter_input_introspection<T>::size > 0)
struct smooth_param_storage<T>
{
  // std::tuple< smooth_param_storage_type<field1>, smooth_param_storage_type<field3>, ... >
  using tuple = filter_and_apply<
      smooth_param_storage_type, smooth_parameter_input_introspection, T>;

  [[no_unique_address]] tuple smoothed_inputs;
};

/**
 * Used to store buffers for sample-accurate parameters.
 * These associate timestamps with value changes.
 */
template <typename T>
struct smooth_storage : smooth_param_storage<T>
{
  using smooth_in = smooth_parameter_input_introspection<T>;

  void init(avnd::effect_container<T>& t, double sample_rate)
  {
    if constexpr(smooth_in::size > 0)
    {
      const auto ratio = -2. * avnd::pi / (1e-3f * sample_rate);

      auto init_smooth = [&]<auto Idx, typename M>(M & port, avnd::predicate_index<Idx>)
      {
        auto& buf = tpl::get<Idx>(this->smoothed_inputs);

        constexpr auto smooth = avnd::get_smooth<M>();

        if constexpr(requires { smooth.ratio(0.); })
        {
          buf.smooth_rate = smooth.ratio(sample_rate);
        }
        else if constexpr(requires { smooth.duration; })
        {
          static_assert(smooth.duration > std::chrono::milliseconds(0));
          buf.smooth_rate = std::exp(ratio / std::chrono::duration_cast<std::chrono::milliseconds>(smooth.duration).count());
        }
        else
        {
          static_assert(smooth.milliseconds > 0.);
          buf.smooth_rate = std::exp(ratio / M::smooth_ratio());
        }
      };
      smooth_in::for_all_n(avnd::get_inputs(t), init_smooth);
    }
  }

  template <avnd::smooth_parameter Field, typename Val, std::size_t NField>
  void update_target(Field& field, const Val& next, avnd::field_index<NField> idx)
  {
    static constexpr auto npredicate = smooth_in::template unmap<NField>();
    smooth_param_storage_type<Field>& storage = get<npredicate>(this->smoothed_inputs);

    storage.target_value = next;
  }

  void smooth_all(avnd::effect_container<T>& t)
  {
    if constexpr(smooth_in::size > 0)
    {
      auto process_smooth
          = [this]<auto Idx, typename M>(M & field, avnd::predicate_index<Idx> idx)
      {
        smooth_param_storage_type<M>& storage = get<Idx>(this->smoothed_inputs);

        storage.current_value
            = storage.target_value
              - storage.smooth_rate * (storage.target_value - storage.current_value);
        field.value = storage.current_value;
      };
      smooth_in::for_all_n(avnd::get_inputs(t), process_smooth);
    }
  }
};
}
