#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/parameter_setup.hpp>

// This file contains the implementation of parameter setup methods
// It requires the actual TouchDesigner headers to be included

#if __has_include("CPlusPlus_Common.h")

#include "CPlusPlus_Common.h"

namespace touchdesigner
{

template <typename T>
template <typename Field>
void parameter_setup<T>::setup_float_parameter(const char* name, const char* label)
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

template <typename T>
template <typename Field>
void parameter_setup<T>::setup_int_parameter(const char* name, const char* label)
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

template <typename T>
template <typename Field>
void parameter_setup<T>::setup_bool_parameter(const char* name, const char* label)
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

template <typename T>
template <typename Field>
void parameter_setup<T>::setup_string_parameter(const char* name, const char* label)
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

template <typename T>
template <typename Field>
void parameter_setup<T>::setup_enum_parameter(const char* name, const char* label)
{
  TD::OP_StringParameter param(name);
  param.label = label;

  // TODO: Extract enum values and create menu
  // For now, create as a simple string parameter
  // Will need magic_enum or similar to enumerate values

  manager->appendString(param);
}

template <typename T>
template <typename Field>
void parameter_setup<T>::setup_pulse_parameter(const char* name, const char* label)
{
  TD::OP_NumericParameter param(name);
  param.label = label;

  manager->appendPulse(param);
}

} // namespace touchdesigner

#endif // has CPlusPlus_Common.h
