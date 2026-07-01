#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/common/aggregates.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/range.hpp>
#include <avnd/introspection/widgets.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <CPlusPlus_Common.h>
#include <avnd/common/enum_reflection.hpp>

#include <string>
#include <string_view>
#include <vector>

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
      // Widget controls (sliders, xy, colour, menus, ...) -> TD parameters.
      avnd::control_input_introspection<T>::for_all(
          avnd::get_inputs(impl),
          [this]<typename Field>(Field& field) {
            setup_parameter<Field>();
          });

      // Value-ports are parameters WITHOUT a widget (e.g. an int/float control
      // with just a range, like an audio effect's amount). They are the natural
      // TD parameters for those objects too, so expose the scalar ones -- else
      // there is no way to set them (they were silently dropped, so audio
      // effects had no adjustable controls).
      avnd::parameter_input_introspection<T>::for_all(
          avnd::get_inputs(impl),
          [this]<typename Field>(Field& field) {
            using vt = std::decay_t<decltype(Field::value)>;
            if constexpr(
                avnd::value_port<Field>
                && (std::is_arithmetic_v<vt> || std::is_enum_v<vt>
                    || avnd::string_ish<vt>))
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
    if constexpr(touchdesigner::string_menu_param<Field>)
      setup_string_menu<Field>(name.data(), label.data());
    else if constexpr(avnd::enum_ish_parameter<Field>)
      setup_enum<Field>(name.data(), label.data());
    else
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

    // halp::maintained_button maps to a momentary (held) button; a plain
    // toggle/checkbox stays a toggle.
    if constexpr(touchdesigner::momentary_button_param<Field>)
      manager->appendMomentary(param);
    else
      manager->appendToggle(param);
  }

  template <avnd::string_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_StringParameter param(name);
    param.label = label;

    // Default value: only usable if the range exposes a string-ish `init`
    // (e.g. halp::lineedit). Folder ports and bare std::string have none.
    if constexpr(requires {
                   std::string_view{avnd::get_range<Field>().init};
                 })
    {
      static constexpr auto init = avnd::get_range<Field>();
      param.defaultValue = std::string_view{init.init}.data();
    }
    else
    {
      param.defaultValue = "";
    }

    // halp::folder_port requests a directory picker.
    if constexpr(touchdesigner::folder_param<Field>)
      manager->appendFolder(param);
    else
      manager->appendString(param);
  }

  // String-valued fixed choices (halp::string_enum_t): use a TD string menu so
  // the stored parameter value is the chosen string itself.
  template <avnd::string_parameter Field>
  void setup_string_menu(const char* name, const char* label)
  {
    TD::OP_StringParameter param(name);
    param.label = label;

    static constexpr auto count = avnd::get_enum_choices_count<Field>();
    static_assert(count > 0);
    static constexpr auto choices = avnd::get_enum_choices<Field>();

    // TD copies the strings during the append call, so temporaries are fine.
    std::vector<std::string> storage;
    storage.reserve(count);
    for(int i = 0; i < count; ++i)
      storage.emplace_back(choices[i]);

    std::vector<const char*> names;
    names.reserve(count);
    for(auto& s : storage)
      names.push_back(s.c_str());

    if(!storage.empty())
      param.defaultValue = storage.front().c_str();

    manager->appendStringMenu(param, count, names.data(), names.data());
  }

  template <avnd::enum_ish_parameter Field>
  void setup_enum(const char* name, const char* label)
  {
    if constexpr(avnd::enum_parameter<Field>)
    {
      // actual enum
      using enum_type = std::decay_t<decltype(Field::value)>;
      TD::OP_NumericParameter param(name);
      param.label = label;

      static constexpr auto enum_values = avnd::enum_values<enum_type>();
      static_assert(std::ssize(enum_values) > 0);
      static constexpr auto enum_min = *std::min_element(std::begin(enum_values), std::end(enum_values));
      static constexpr auto enum_max = *std::max_element(std::begin(enum_values), std::end(enum_values));
      param.minValues[0] = std::to_underlying(enum_min);
      param.maxValues[0] = std::to_underlying(enum_max);
      param.minSliders[0] = std::to_underlying(enum_min);
      param.maxSliders[0] = std::to_underlying(enum_max);

      if constexpr (avnd::has_range<Field>)
      {
        static constexpr auto range = avnd::get_range<Field>();

        param.defaultValues[0] = std::to_underlying(range.init);
        param.clampMins[0] = true;
        param.clampMaxes[0] = true;
      }
      else
      {
        param.defaultValues[0] = std::to_underlying(enum_values[0]);
      }

      manager->appendDynamicMenu(param);
    }
    else
    {
      // enum_ish: string-list or combo_pair-based choices
      static constexpr auto count = avnd::get_enum_choices_count<Field>();
      static_assert(count > 0);

      TD::OP_NumericParameter param(name);
      param.label = label;
      param.minValues[0] = 0;
      param.maxValues[0] = count - 1;
      param.minSliders[0] = 0;
      param.maxSliders[0] = count - 1;
      param.clampMins[0] = true;
      param.clampMaxes[0] = true;

      if constexpr(avnd::has_range<Field>)
      {
        static constexpr auto range = avnd::get_range<Field>();
        param.defaultValues[0] = range.init;
      }
      else
      {
        param.defaultValues[0] = 0;
      }

      manager->appendDynamicMenu(param);
    }
  }

  template <avnd::xy_parameter Field>
    requires (!avnd::xyz_parameter<Field>)
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    for(int i = 0; i < 2; i++) {
    // Check if field has range metadata
      if constexpr (avnd::parameter_with_minmax_range<Field>)
      {
  
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
    requires (!avnd::xyzw_parameter<Field>)
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    for(int i = 0; i < 3; i++) {
      // Check if field has range metadata
      if constexpr (avnd::parameter_with_minmax_range<Field>)
      {

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

  template <avnd::xyzw_parameter Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    for(int i = 0; i < 4; i++) {
      // Check if field has range metadata
      if constexpr (avnd::parameter_with_minmax_range<Field>)
      {

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

    manager->appendXYZW(param);
  }

  template <avnd::uv_parameter Field>
    requires (!avnd::uvw_parameter<Field>)
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    for(int i = 0; i < 2; i++) {
      // Check if field has range metadata
      if constexpr (avnd::parameter_with_minmax_range<Field>)
      {
  
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
    requires (!avnd::rgba_parameter<Field>)
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    for(int i = 0; i < 3; i++) {
      // Check if field has range metadata
      if constexpr (avnd::parameter_with_minmax_range<Field>)
      {
  
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

    for(int i = 0; i < 4; i++) {
      // Check if field has range metadata
      if constexpr (avnd::parameter_with_minmax_range<Field>)
      {
  
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

  // Generic fallback: impulse_button / bang and other trigger-like ports with
  // no specific value type map to a TD pulse button.
  template <typename Field>
  void setup_parameter(const char* name, const char* label)
  {
    TD::OP_NumericParameter param(name);
    param.label = label;

    manager->appendPulse(param);
  }

  // NOTE: file_port / soundfile_port / midifile_port are NOT parameter_port
  // (they have no `.value`) so they are not iterated here. They are handled by
  // touchdesigner::file_ports<T> (file_ports.hpp), which appends the File
  // parameter and loads the file contents on cook.
};

}
