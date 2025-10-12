#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/common/aggregates.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/range.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <CPlusPlus_Common.h>

// TouchDesigner headers
// We need the actual TD headers here for parameter management
// These should be available when implementing a TD plugin

namespace TD {
  class OP_ParameterManager;
  class OP_NumericParameter;
  class OP_StringParameter;
  enum class OP_ParAppendResult : int32_t;
}

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
    constexpr auto name = get_td_name<Field>();
    constexpr auto label = avnd::get_name<Field>();

    // Float parameters
    if constexpr (avnd::float_parameter<Field>)
    {
      setup_float_parameter<Field>(name.data(), label.data());
    }
    // Int parameters
    else if constexpr (avnd::int_parameter<Field>)
    {
      setup_int_parameter<Field>(name.data(), label.data());
    }
    // Bool parameters (toggle)
    else if constexpr (avnd::bool_parameter<Field>)
    {
      setup_bool_parameter<Field>(name.data(), label.data());
    }
    // String parameters
    else if constexpr (avnd::string_parameter<Field>)
    {
      setup_string_parameter<Field>(name.data(), label.data());
    }
    // Enum parameters (menu)
    else if constexpr (avnd::enum_parameter<Field>)
    {
      setup_enum_parameter<Field>(name.data(), label.data());
    }
    // Button/pulse parameters
    else if constexpr (requires { Field::is_button; } || requires { Field::is_pulse; })
    {
      setup_pulse_parameter<Field>(name.data(), label.data());
    }
  }

  template <typename Field>
  void setup_float_parameter(const char* name, const char* label)
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

  template <typename Field>
  void setup_int_parameter(const char* name, const char* label)
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

  template <typename Field>
  void setup_bool_parameter(const char* name, const char* label)
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

  template <typename Field>
  void setup_string_parameter(const char* name, const char* label)
  {
    TD::OP_StringParameter param(name);
    param.label = label;

    // Set default value if available
    if constexpr (requires { Field::value; })
    {
      // For compile-time string, we'd need to extract it
      // For now, leave as nullptr (empty default)
      param.defaultValue = nullptr;
    }
    else
    {
      param.defaultValue = nullptr;
    }

    manager->appendString(param);
  }

  template <typename Field>
  void setup_enum_parameter(const char* name, const char* label)
  {
    TD::OP_StringParameter param(name);
    param.label = label;

    // TODO: Extract enum values and create menu
    // For now, create as a simple string parameter
    // Will need magic_enum or similar to enumerate values

    manager->appendString(param);
  }

  template <typename Field>
  void setup_pulse_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    manager->appendPulse(param);
  }
};

}
