#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/avnd.hpp>
#include <avnd/binding/vintage/helpers.hpp>
#include <avnd/binding/vintage/vintage.hpp>

namespace vintage
{

template <typename T>
concept can_dispatch = requires(T t)
{
  t.dispatch(nullptr, 0, 0, 0, nullptr, 0.f);
};

template <typename T>
concept can_event = requires(T eff)
{
  eff.midi_input(std::declval<const vintage::MidiEvent&>());
}
|| requires(T eff)
{
  eff.event_input({});
};

template <typename Self_T>
intptr_t default_dispatch(
    Self_T& object,
    EffectOpcodes opcode,
    int32_t index,
    intptr_t value,
    void* ptr,
    float opt)
{
  using effect_type = typename decltype(object.effect)::type;
  auto& container = object.effect;
  // std::cerr << int(opcode) << ": " << index << " ; " << value << " ;" << ptr
  //           << " ;" << opt << std::endl;

  // C++20: Not supported by clang-13 yet :-(
  // using enum EffectOpcodes;

  switch (opcode)
  {
    case EffectOpcodes::Identify: // 22
      return 0;
    case EffectOpcodes::SetProcessPrecision: // 77
    {
      object.precision
          = value ? ProcessPrecision::Double : ProcessPrecision::Single;
      return 1;
    }
    case EffectOpcodes::SetBlockSizeAndSampleRate: // 43
    {
      object.sample_rate = opt;
      object.buffer_size = value;
      return 1;
    }
    case EffectOpcodes::SetSampleRate: // 10
    {
      object.sample_rate = opt;
      return 1;
    }
    case EffectOpcodes::SetBlockSize: // 11
    {
      object.buffer_size = value;
      return 1;
    }
    case EffectOpcodes::Open: // 0
    {
      object.start();
      return 1;
    }
    case EffectOpcodes::GetPlugCategory: // 35
    {
      if constexpr (requires { effect_type::category; })
        return static_cast<int32_t>(container.category);
      return 0;
    }

    case EffectOpcodes::ConnectInput: // 31
      return 1;
    case EffectOpcodes::ConnectOutput: // 32
      return 1;

    case EffectOpcodes::GetMidiKeyName: // 66
      return 1;

    case EffectOpcodes::GetProgram: // 3
    {
      if constexpr (avnd::has_programs<effect_type>)
      {
        return object.current_program;
      }
      return 0;
    }

    case EffectOpcodes::SetProgram: // 2
    {
      if constexpr (avnd::has_programs<effect_type>)
      {
        if (value >= 0 && value < std::ssize(effect_type::programs))
        {
          object.current_program = value;
          object.controls.read(effect_type::programs[value].parameters);
          object.controls.write(container);
          object.request(HostOpcodes::UpdateDisplay, 0, 0, nullptr, 0.f);
        }
      }
      return 0;
    }

    case EffectOpcodes::BeginSetProgram: // 67
    {
      break;
    }
    case EffectOpcodes::EndSetProgram: // 68
    {
      break;
    }

    case EffectOpcodes::SetBypass: // 44
    {
      if constexpr (avnd::can_bypass<effect_type>)
      {
        container.bypass = bool(value);
      }
      break;
    }
    case EffectOpcodes::MainsChanged: // 12
    {
      if (value)
        object.start();
      else
        object.stop();
      return 1;
    }
    case EffectOpcodes::StartProcess: // 71
    {
      return 1;
    }
    case EffectOpcodes::StopProcess: // 72
      return 1;

    case EffectOpcodes::GetInputProperties: // 33
      return 0;
    case EffectOpcodes::GetOutputProperties: // 34
      return 0;
    case EffectOpcodes::GetParameterProperties: // 56
    {
      if constexpr(avnd::has_inputs<effect_type>)
      {
        auto& props = *(vintage::ParameterProperties*)ptr;

        avnd::float_parameter_input_introspection<effect_type>::for_nth(
            container.inputs(), index,
            [&props, index](const auto& param)
            {
              props.stepFloat = 0.01;
              props.smallStepFloat = 0.01;
              props.largeStepFloat = 0.01;

              if constexpr (requires { param.label(); })
              {
                vintage::label{param.label()}.copy_to(props.label);
              }
              props.flags = {};
              props.minInteger = 0;
              props.maxInteger = 1;
              props.stepInteger = 1;
              props.largeStepInteger = 1;

              if constexpr (requires { param.shortLabel(); })
              {
                vintage::short_label{param.shortLabel()}.copy_to(
                    props.shortLabel);
              }
              props.displayIndex = index;
              props.category = 0;
              props.numParametersInCategory = 2;
              if constexpr (requires { param.categoryLabel(); })
              {
                vintage::category_label{param.categoryLabel()}.copy_to(
                    props.categoryLabel);
              }
            });
      }
      return 1;
    }
    case EffectOpcodes::CanBeAutomated: // 26
      return 1;
    case EffectOpcodes::GetEffectName: // 45
    {
      // Should be able to pass it directly, but apparently GCC bugs...
      constexpr std::string_view name = effect_type::name();
      vintage::name{name}.copy_to(ptr);
      return 1;
    }

    case EffectOpcodes::GetVendorString: // 47
    {
      if constexpr (requires { effect_type::vendor(); })
      {
        vintage::vendor_name{effect_type::vendor()}.copy_to(ptr);
        return 1;
      }
      else
      {
        return 0;
      }
    }

    case EffectOpcodes::GetProductString: // 48
    {
      if constexpr (requires { effect_type::product(); })
      {
        vintage::product_name{effect_type::product()}.copy_to(ptr);
        return 1;
      }
      else
      {
        return 0;
      }
      return 1;
    }

    case EffectOpcodes::GetProgramName: // 5
    {
      if constexpr (avnd::has_programs<effect_type>)
      {
        if (object.current_program < std::ssize(effect_type::programs))
        {
          vintage::program_name{
              effect_type::programs[object.current_program].name}
              .copy_to(ptr);
          return 1;
        }
      }
      break;
    }

    case EffectOpcodes::GetProgramNameIndexed: // 29
    {
      if constexpr (avnd::has_programs<effect_type>)
      {
        if (index >= 0 && index < std::ssize(effect_type::programs))
        {
          vintage::program_name{effect_type::programs[index].name}.copy_to(
              ptr);
          return 1;
        }
      }
      break;
    }

    case EffectOpcodes::GetParamLabel: // 6
    {
      object.controls.label(object, index, ptr);
      return 1;
    }

    case EffectOpcodes::GetParamName: // 8
    {
      object.controls.name(object, index, ptr);
      return 1;
    }

    case EffectOpcodes::GetParamDisplay: // 7
    {
      object.controls.display(object, index, ptr);
      return 1;
    }

    case EffectOpcodes::ProcessEvents:
    {
      auto evs = reinterpret_cast<const vintage::Events*>(ptr);
      if constexpr (requires {
                      object.midi_input(
                          std::declval<const vintage::MidiEvent&>());
                    })
      {
        for (int32_t i = 0, n = evs->numEvents; i < n; i++)
        {
          const auto* ev = evs->events[i];
          switch (ev->type)
          {
            case vintage::EventTypes::Midi:
              object.midi_input(
                  *reinterpret_cast<const vintage::MidiEvent*>(ev));
              break;
            default:
              break;
          }
        }
      }
      else if constexpr (requires { object.event_input(evs); })
      {
        object.event_input(evs);
      }
      return 1;
    }
    case EffectOpcodes::GetVendorVersion: // 49
    {
      if constexpr (requires { effect_type::version; })
        return effect_type::version;
      return 0;
    }
    case EffectOpcodes::GetApiVersion: // 58
      return Constants::ApiVersion;
    case EffectOpcodes::CanDo: // 51
    {
      if constexpr (can_event<Effect>)
      {
        static const std::set<std::string_view> available{
            //"MPE",
            //"hasCockosExtensions",
            "receiveVstEvents",
            "receiveVstMidiEvent",
            "receiveVstSysexEvent",
        };
        return available.find(reinterpret_cast<const char*>(ptr))
                       != available.end()
                   ? 1
                   : 0;
      }
      /*
      "sendVstEvents";
      "sendVstMidiEvent";
      "bypass";
      fmt::print(" --> Can do ? {} \n", (const char*)ptr);
      */
      return 0;
    }

    default:
      break;
  }
  return 0;
}

}
