#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/common/aggregates.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/range.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <CPlusPlus_Common.h>
#include <magic_enum/magic_enum.hpp>

namespace touchdesigner
{
/**
 * Helper to set up parameters from an Avendish processor's inputs
 * Maps Avendish parameter concepts to TouchDesigner parameter types
 */
template <typename T>
struct parameter_setup
{
  TD::OP_ParameterManager* manager{};

  void setup(avnd::effect_container<T>& impl, TD::OP_ParameterManager* mgr)
  {
    manager = mgr;

    if constexpr(avnd::has_inputs<T>) {
      // Iterate over all input fields that are parameters
      avnd::parameter_input_introspection<T>::for_all(
          avnd::get_inputs(impl),
          [this]<typename Field>(Field& field) {
            setup_parameter<Field>();
          });
    }
  }

private:
  template <typename Field>
  void setup_parameter()
  {
    static constexpr auto name = get_td_name<Field>();
    static constexpr auto label = get_parameter_label<Field>();
    setup_parameter<Field>(name.data(), label.data());
  }

  template <avnd::float_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    // Check if field has range metadata
    if constexpr (avnd::parameter_with_minmax_range<Field>)
    {
      // FIXME enum
      static constexpr auto range = avnd::get_range<Field>();
      param.defaultValues[0] = range.init;
      param.minValues[0] = range.min;
      param.maxValues[0] = range.max;
      param.minSliders[0] = range.min;
      param.maxSliders[0] = range.max;
      param.clampMins[0] = true;
      param.clampMaxes[0] = true;
    }
    else
    {
      // Default float range
      param.defaultValues[0] = 0.0;
      param.minValues[0] = 0.0;
      param.maxValues[0] = 1.0;
      param.minSliders[0] = 0.0;
      param.maxSliders[0] = 1.0;
    }

    manager->appendFloat(param);
  }

  template <avnd::int_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    if constexpr (avnd::parameter_with_minmax_range<Field>)
    {
      // FIXME enum
      static constexpr auto range = avnd::get_range<Field>();
      param.defaultValues[0] = range.init;
      param.minValues[0] = range.min;
      param.maxValues[0] = range.max;
      param.minSliders[0] = range.min;
      param.maxSliders[0] = range.max;
      param.clampMins[0] = true;
      param.clampMaxes[0] = true;
    }
    else
    {
      param.defaultValues[0] = 0.0;
      param.minValues[0] = 0.0;
      param.maxValues[0] = 100.0;
      param.minSliders[0] = 0.0;
      param.maxSliders[0] = 100.0;
    }

    manager->appendInt(param);
  }

  template <avnd::bool_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    // Get default value if available
    if constexpr (requires { Field::value; })
    {
      param.defaultValues[0] = Field{}.value ? 1.0 : 0.0;
    }
    else
    {
      param.defaultValues[0] = 0.0;
    }

    manager->appendToggle(param);
  }

  template <avnd::string_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_StringParameter param(name);
    param.label = label;

    if constexpr (avnd::has_range<Field>)
    {
      static constexpr auto init = avnd::get_range<Field>();
      param.defaultValue = init.init.data();
    }
    else
    {
      param.defaultValue = "";
    }

    manager->appendString(param);
  }

  template <avnd::enum_ish_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    if constexpr(avnd::enum_parameter<Field>)
    {
      // actual enum
      using enum_type = std::decay_t<decltype(Field::value)>;
      TD::OP_NumericParameter param(name);
      param.label = label;

      if constexpr (avnd::has_range<Field>)
      {
        static constexpr auto range = avnd::get_range<Field>();
        static constexpr auto colors = magic_enum::enum_values<enum_type>();
        static_assert(std::ssize(colors) > 0);

        param.defaultValues[0] = std::to_underlying(range.init);
        param.minValues[0] = std::to_underlying(colors[0]);
        param.maxValues[0] = std::to_underlying(colors[colors.size() - 1]);
        param.minSliders[0] = std::to_underlying(colors[0]);
        param.maxSliders[0] = std::to_underlying(colors[colors.size() - 1]);
        param.clampMins[0] = true;
        param.clampMaxes[0] = true;
      }
      else
      {
        param.defaultValues[0] = 0.0;
        param.minValues[0] = 0.0;
        param.maxValues[0] = 100.0;
        param.minSliders[0] = 0.0;
        param.maxSliders[0] = 100.0;
      }

      manager->appendDynamicMenu(param);
    }
    else
    {
      // actual enum
      TD::OP_StringParameter param(name);
      param.label = label;

      manager->appendDynamicStringMenu(param);
    }
  }

  template <avnd::xy_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    for(int i = 0; i < 2; i++) {
    // Check if field has range metadata
      if constexpr (avnd::parameter_with_minmax_range<Field>)
      {
        // FIXME enum
        static constexpr auto range = avnd::get_range<Field>();
        param.defaultValues[i] = range.init;
        param.minValues[i] = range.min;
        param.maxValues[i] = range.max;
        param.minSliders[i] = range.min;
        param.maxSliders[i] = range.max;
        param.clampMins[i] = true;
        param.clampMaxes[i] = true;
      }
      else
      {
        // Default float range
        param.defaultValues[i] = 0.0;
        param.minValues[i] = 0.0;
        param.maxValues[i] = 1.0;
        param.minSliders[i] = 0.0;
        param.maxSliders[i] = 1.0;
      }
    }

    manager->appendXY(param);
  }

  template <avnd::xyz_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    for(int i = 0; i < 3; i++) {
      // Check if field has range metadata
      if constexpr (avnd::parameter_with_minmax_range<Field>)
      {
        // FIXME enum
        static constexpr auto range = avnd::get_range<Field>();
        param.defaultValues[i] = range.init;
        param.minValues[i] = range.min;
        param.maxValues[i] = range.max;
        param.minSliders[i] = range.min;
        param.maxSliders[i] = range.max;
        param.clampMins[i] = true;
        param.clampMaxes[i] = true;
      }
      else
      {
        // Default float range
        param.defaultValues[i] = 0.0;
        param.minValues[i] = 0.0;
        param.maxValues[i] = 1.0;
        param.minSliders[i] = 0.0;
        param.maxSliders[i] = 1.0;
      }
    }

    manager->appendXYZ(param);
  }

  template <avnd::uv_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    for(int i = 0; i < 2; i++) {
      // Check if field has range metadata
      if constexpr (avnd::parameter_with_minmax_range<Field>)
      {
        // FIXME enum
        static constexpr auto range = avnd::get_range<Field>();
        param.defaultValues[i] = range.init;
        param.minValues[i] = range.min;
        param.maxValues[i] = range.max;
        param.minSliders[i] = range.min;
        param.maxSliders[i] = range.max;
        param.clampMins[i] = true;
        param.clampMaxes[i] = true;
      }
      else
      {
        // Default float range
        param.defaultValues[i] = 0.0;
        param.minValues[i] = 0.0;
        param.maxValues[i] = 1.0;
        param.minSliders[i] = 0.0;
        param.maxSliders[i] = 1.0;
      }
    }

    manager->appendUV(param);
  }

  template <avnd::uvw_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    for(int i = 0; i < 3; i++) {
      // Check if field has range metadata
      if constexpr (avnd::parameter_with_minmax_range<Field>)
      {
        // FIXME enum
        static constexpr auto range = avnd::get_range<Field>();
        param.defaultValues[i] = range.init;
        param.minValues[i] = range.min;
        param.maxValues[i] = range.max;
        param.minSliders[i] = range.min;
        param.maxSliders[i] = range.max;
        param.clampMins[i] = true;
        param.clampMaxes[i] = true;
      }
      else
      {
        // Default float range
        param.defaultValues[i] = 0.0;
        param.minValues[i] = 0.0;
        param.maxValues[i] = 1.0;
        param.minSliders[i] = 0.0;
        param.maxSliders[i] = 1.0;
      }
    }

    manager->appendUVW(param);
  }

  template <avnd::rgb_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    for(int i = 0; i < 3; i++) {
      // Check if field has range metadata
      if constexpr (avnd::parameter_with_minmax_range<Field>)
      {
        // FIXME enum
        static constexpr auto range = avnd::get_range<Field>();
        param.defaultValues[i] = range.init;
        param.minValues[i] = range.min;
        param.maxValues[i] = range.max;
        param.minSliders[i] = range.min;
        param.maxSliders[i] = range.max;
        param.clampMins[i] = true;
        param.clampMaxes[i] = true;
      }
      else
      {
        // Default float range
        param.defaultValues[i] = 0.0;
        param.minValues[i] = 0.0;
        param.maxValues[i] = 1.0;
        param.minSliders[i] = 0.0;
        param.maxSliders[i] = 1.0;
      }
    }

    manager->appendRGB(param);
  }

  template <avnd::rgba_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    for(int i = 0; i < 3; i++) {
      // Check if field has range metadata
      if constexpr (avnd::parameter_with_minmax_range<Field>)
      {
        // FIXME enum
        static constexpr auto range = avnd::get_range<Field>();
        param.defaultValues[i] = range.init;
        param.minValues[i] = range.min;
        param.maxValues[i] = range.max;
        param.minSliders[i] = range.min;
        param.maxSliders[i] = range.max;
        param.clampMins[i] = true;
        param.clampMaxes[i] = true;
      }
      else
      {
        // Default float range
        param.defaultValues[i] = 0.0;
        param.minValues[i] = 0.0;
        param.maxValues[i] = 1.0;
        param.minSliders[i] = 0.0;
        param.maxSliders[i] = 1.0;
      }
    }

    manager->appendRGBA(param);
  }

  template <typename Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    manager->appendPulse(param);
  }

  template <avnd::file_port Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_StringParameter param(name);
    param.label = label;

    if constexpr (avnd::has_range<Field>)
    {
      static constexpr auto init = avnd::get_range<Field>();
      param.defaultValue = init.c_str();
    }
    else
    {
      param.defaultValue = "";
    }

    manager->appendString(param);
  }
};

}
