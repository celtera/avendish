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
};

}
