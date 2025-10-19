#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/common/aggregates.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/introspection/range.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <CPlusPlus_Common.h>
#include <magic_enum/magic_enum.hpp>

namespace touchdesigner
{
template <typename T>
struct parameter_update
{
  void pulse(avnd::effect_container<T>& implementation, const char* name)
  {
    avnd::control_input_introspection<T>::for_all(
        avnd::get_inputs(implementation),
        [&]<typename Field>(Field& field) {
      static constexpr auto name = touchdesigner::get_td_name<Field>();
      this->pulse(field, name.data());
      if_possible(field.update(implementation.effect));
    });
  }

  template <typename Field>
  void pulse(Field& field, const char* name)
  {
    using type = std::decay_t<decltype(Field::value)>;
    if constexpr(avnd::optional_ish<type>) {
      field.value = type{std::in_place};
    }
  }

  void menu(avnd::effect_container<T>& implementation, const TD::OP_Inputs* inputs, TD::OP_BuildDynamicMenuInfo* info)
  {
    avnd::control_input_introspection<T>::for_all(
        avnd::get_inputs(implementation),
        [&]<typename Field>(Field& field) {
      static constexpr auto name = touchdesigner::get_td_name<Field>();
      this->menu(field, name.data(),  info);
    });
  }

  template <avnd::enum_ish_parameter Field>
  void menu(Field& field, const char* name,  TD::OP_BuildDynamicMenuInfo* info)
  {
    if(strcmp(name, info->name) == 0)
    {
      if constexpr(avnd::enum_parameter<Field>)
      {
        using enum_type = std::decay_t<decltype(Field::value)>;
        static constexpr auto enum_values = magic_enum::enum_values<enum_type>();
        static constexpr auto enum_entries = magic_enum::enum_entries<enum_type>();
        if constexpr (avnd::has_range<Field>)
        {
          static constexpr auto range = avnd::get_range<Field>();
          for(auto& label : range.values)
          {
            info->addMenuEntry(label.data(), label.data());
          }
        }
        else
        {
          for(auto [val, name] : enum_entries) {
            std::string_view nm{name};
            info->addMenuEntry(nm.data(), nm.data());
          }
        }
      }
      else
      {
        static constexpr auto range = avnd::get_range<Field>();
        for(auto& label : range.values)
        {
          info->addMenuEntry(label.data(), label.data());
        }
      }
    }
  }

  template <typename Field>
  void menu(Field& field, const char* name,  TD::OP_BuildDynamicMenuInfo* info)
  {

  }

  void update(avnd::effect_container<T>& implementation, const TD::OP_Inputs* inputs)
  {
    if constexpr(avnd::has_inputs<T>) {
      // Iterate over all parameter inputs and update from TD
      avnd::control_input_introspection<T>::for_all(
          avnd::get_inputs(implementation),
          [&]<typename Field>(Field& field) {
        static constexpr auto name = touchdesigner::get_td_name<Field>();
        this->update(field, name.data(), inputs);

        // Call update callback if it exists
        if_possible(field.update(implementation.effect));
      });
    }
  }

  template <avnd::float_parameter Field>
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    field.value = static_cast<float>(inputs->getParDouble(name));
  }

  template <avnd::int_parameter Field>
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    field.value = inputs->getParInt(name);
  }

  template <avnd::bool_parameter Field>
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    field.value = inputs->getParInt(name) != 0;
  }

  template <avnd::string_parameter Field>
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    const char* str = inputs->getParString(name);
    if(str)
      field.value.assign(str);
  }

  template <avnd::enum_ish_parameter Field>
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    if constexpr(avnd::enum_parameter<Field>)
    {
      using enum_type = std::decay_t<decltype(Field::value)>;
      field.value = static_cast<enum_type>(inputs->getParInt(name));
    }
    else
    {
      const char* str = inputs->getParString(name);
      if(str)
        field.value.assign(str);
    }
  }

  template <avnd::xy_parameter Field>
    requires (!avnd::xyz_parameter<Field>)
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    using type = std::decay_t<decltype(field.value.x)>;
    if constexpr(std::is_integral_v<type>) {
      int res[2];
      inputs->getParInt2(name, res[0], res[1]);
      field.value = {(type)res[0], (type)res[1]};
    }
    else
    {
      double res[2];
      inputs->getParDouble2(name, res[0], res[1]);
      field.value = {(type)res[0], (type)res[1]};
    }
  }

  template <avnd::xyz_parameter Field>
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    using type = std::decay_t<decltype(field.value.x)>;
    if constexpr(std::is_integral_v<type>) {
      int res[3];
      inputs->getParInt3(name, res[0], res[1], res[2]);
      field.value = {(type)res[0], (type)res[1], (type)res[2]};
    }
    else
    {
      double res[3];
      inputs->getParDouble3(name, res[0], res[1], res[2]);
      field.value = {(type)res[0], (type)res[1], (type) res[2]};
    }
  }

  template <avnd::uv_parameter Field>
    requires (!avnd::uvw_parameter<Field>)
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    using type = std::decay_t<decltype(field.value.u)>;
    if constexpr(std::is_integral_v<type>) {
      int res[2];
      inputs->getParInt2(name, res[0], res[1]);
      field.value = {(type)res[0], (type)res[1]};
    }
    else
    {
      double res[2];
      inputs->getParDouble2(name, res[0], res[1]);
      field.value = {(type)res[0], (type)res[1]};
    }
  }

  template <avnd::uvw_parameter Field>
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    using type = std::decay_t<decltype(field.value.u)>;
    if constexpr(std::is_integral_v<type>) {
      int res[3];
      inputs->getParInt3(name, res[0], res[1], res[2]);
      field.value = {(type)res[0], (type)res[1], (type)res[2]};
    }
    else
    {
      double res[3];
      inputs->getParDouble3(name, res[0], res[1], res[2]);
      field.value = {(type)res[0], (type)res[1], (type) res[2]};
    }
  }

  template <avnd::rgb_parameter Field>
    requires (!avnd::rgba_parameter<Field>)
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    using type = std::decay_t<decltype(field.value.r)>;
    if constexpr(std::is_integral_v<type>) {
      int res[3];
      inputs->getParInt3(name, res[0], res[1], res[2]);
      field.value = {(type)res[0], (type)res[1], (type)res[2]};
    }
    else
    {
      double res[3];
      inputs->getParDouble3(name, res[0], res[1], res[2]);
      field.value = {(type)res[0], (type)res[1], (type) res[2]};
    }
  }

  template <avnd::rgba_parameter Field>
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    using type = std::decay_t<decltype(field.value.r)>;
    if constexpr(std::is_integral_v<type>) {
      int res[4];
      inputs->getParInt4(name, res[0], res[1], res[2], res[3]);
      field.value = {(type)res[0], (type)res[1], (type)res[2], (type)res[3]};
    }
    else
    {
      double res[4];
      inputs->getParDouble4(name, res[0], res[1], res[2], res[3]);
      field.value = {(type)res[0], (type)res[1], (type) res[2], (type)res[3]};
    }
  }

  template <typename Field>
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    pulse(field, name);
  }

  template <avnd::file_port Field>
  void update(Field& field, const char* name, const TD::OP_Inputs* inputs)
  {
    const char* str = inputs->getParFilePath(name);
    if(str)
      field.value.assign(str);
  }

  //////////////////
  // We know that input exists and has at least one channel sample

  template <typename Field, typename Value>
    requires (avnd::fp_ish<Value> || avnd::int_ish<Value> || avnd::bool_ish<Value>)
  void chop_value_in(Field& field, Value& value, const TD::OP_CHOPInput* input)
  {
    value = input->channelData[0][0];
  }

  template <typename Field, avnd::enum_ish Value>
  void chop_value_in(Field& field, Value& value, const TD::OP_CHOPInput* input)
  {
    value = static_cast<Value>(input->channelData[0][0]);
  }

  template <typename Field, avnd::list_ish Value>
    requires (!avnd::vector_ish<Value> && !avnd::string_ish<Value>)
  void chop_value_in(Field& field,  Value& value, const TD::OP_CHOPInput* input)
  {
    using value_type = typename Value::value_type;
    if constexpr(std::is_arithmetic_v<value_type>)
    {
      value.clear();
      auto begin = input->channelData[0];
      auto end = input->channelData[0] + input->numSamples;
      value.assign(begin, end);
      for(auto it = begin; it != end; ++it) {
        value.push_back(*it);
      }
    }
    else
    {
      // TODO?
    }
  }

  template <typename Field, avnd::vector_ish Value>
    requires (!avnd::string_ish<Value>)
  void chop_value_in(Field& field,  Value& value, const TD::OP_CHOPInput* input)
  {
    using value_type = typename Value::value_type;
    if constexpr(std::is_arithmetic_v<value_type>)
    {
      value.resize(input->numSamples);
      auto begin = input->channelData[0];
      auto end = input->channelData[0] + input->numSamples;
      value.assign(begin, end);
    }
    else
    {
      // TODO?
    }
  }

  template <typename Field, avnd::set_ish Value>
  void chop_value_in(Field& field,  Value& value, const TD::OP_CHOPInput* input)
  {
    using value_type = typename Value::value_type;
    if constexpr(std::is_arithmetic_v<value_type>)
    {
      value.clear();
      auto begin = input->channelData[0];
      auto end = input->channelData[0] + input->numSamples;
      value.insert(begin, end);
    }
    else
    {
      // TODO set<string> ?
    }
  }

  template <typename Field, avnd::map_ish Value>
  void chop_value_in(Field& field, Value& value, const TD::OP_CHOPInput* input)
  {
    // FIXME what behaviour do we want here? one channel for keys, one for value?
    // or kv kv kv kv ?
    // or just vvvvv ?
  }

  template <typename Field, avnd::variant_ish Value>
  void chop_value_in(Field& field,  Value& value, const TD::OP_CHOPInput* input)
  {
    if_possible(value = input->channelData[0][0]);
  }

  template <typename Field, avnd::optional_ish Value>
  void chop_value_in(Field& field, Value& value, const TD::OP_CHOPInput* input)
  {
    if_possible(value = input->channelData[0][0]);
  }

  template <typename Field, typename Value>
    requires (std::is_aggregate_v<Value> || avnd::tuple_ish<Value>)
  void chop_value_in(Field& field, Value& value, const TD::OP_CHOPInput* input)
  {
    if constexpr(avnd::pair_ish<Value>)
    {
      value.first = input->channelData[0][0];
      if(input->numSamples > 1)
        value.first = input->channelData[0][1];
    }
    else if constexpr(avnd::static_array_ish<Value>)
    {
      using value_type = std::decay_t<decltype(value[0])>;
      if constexpr(std::is_arithmetic_v<value_type>)
      {
        static constexpr int N = std::tuple_size_v<Value>;
        for(int i = 0, n = std::min(N, (int)input->numSamples); i < n; i++) {
          value[i] = input->channelData[0][i];
        }
      }
    }
    else if constexpr(avnd::tuple_ish<Value>)
    {
      // TODO
    }
    else
    {
      // TODO
    }
  }

  template <typename Field, typename Value>
    requires avnd::string_ish<Value>
  void chop_value_in(Field& field, Value& value, const TD::OP_CHOPInput* input)
  {
    // TODO
  }

  template <typename Field, typename Value>
    requires avnd::bitset_ish<Value>
  void chop_value_in(Field& field, Value& value, const TD::OP_CHOPInput* input)
  {
    if constexpr(requires { value.resize(1); }) {
      value.resize(input->numSamples);
      for(int i = 0, N = input->numSamples; i < N; i++) {
        value.set(i, input->channelData[0] != 0);
      }
    }
    else
    {
      for(int i = 0, N = std::min((int)value.size(), (int)input->numSamples); i < N; i++) {
        value.set(i, input->channelData[0] != 0);
      }
    }
  }

  template <avnd::parameter Field>
  void chop_in(Field& field, const TD::OP_CHOPInput* input)
  {
    chop_value_in(field, field.value, input);
  }

  template <avnd::buffer_port Field>
  void chop_in(Field& field, const TD::OP_CHOPInput* input)
  {
    // TODO
  }
};

}
