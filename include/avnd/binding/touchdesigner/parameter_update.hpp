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

namespace touchdesigner
{
template <typename T>
struct parameter_update
{
  void update(avnd::effect_container<T>& implementation, const TD::OP_Inputs* inputs)
  {
    if constexpr(avnd::has_inputs<T>) {
      // Iterate over all parameter inputs and update from TD
      avnd::parameter_input_introspection<T>::for_all(
          avnd::get_inputs(implementation),
          [&]<typename Field>(Field& field) {
        static constexpr auto name = touchdesigner::get_td_name<Field>();
        update(field, name.data(), inputs);

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
    using type = std::decay_t<decltype(field.r)>;
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
    using type = std::decay_t<decltype(field.r)>;
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
    using type = std::decay_t<decltype(field.r)>;
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
    using type = std::decay_t<decltype(field.r)>;
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
    using type = std::decay_t<decltype(field.r)>;
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
    using type = std::decay_t<decltype(field.r)>;
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
    // FIXME
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
