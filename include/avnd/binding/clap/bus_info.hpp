#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/clap/helpers.hpp>
#include <avnd/wrappers/channels_introspection.hpp>
#include <avnd/wrappers/input_introspection.hpp>
#include <avnd/wrappers/output_introspection.hpp>

#include <clap/all.h>

namespace avnd_clap
{
template<typename T>
struct event_bus_info
{
  using input_refl = avnd::midi_input_introspection<T>;
  using output_refl = avnd::midi_output_introspection<T>;
  static constexpr int input_count() noexcept
  {
    return avnd::midi_input_introspection<T>::size;
  }
  static constexpr int output_count() noexcept
  {
    return avnd::midi_output_introspection<T>::size;
  }

  static bool input_info(
      uint32_t index,
      clap_note_port_info& info)
  {
    static const constexpr int input_event_busses = input_count();

    if(index < input_event_busses) {
      info.id = index;
      input_refl::for_nth_mapped(index, [&] <std::size_t I, typename C> (const avnd::field_reflection<I, C>& port) {
        copy_string(info.name, C::name());
      });
      return true;
    } else {
      return false;
    }
  }

  static bool output_info(
      uint32_t index,
      clap_note_port_info& info)
  {
    static const constexpr int output_event_busses = output_count();

    if(index < output_event_busses) {
      info.id = index;
      output_refl::for_nth_mapped(index, [&] <std::size_t I, typename C> (const avnd::field_reflection<I, C>& port) {
        copy_string(info.name, C::name());
      });
      return true;
    } else {
      return false;
    }
  }
};


// Case where things are passed by argument to the "process" function,
// thus either the number of channels is set explicitely, or we default to 2 as
// that is the expectation for VSTs.
template<typename T>
struct audio_bus_info
{
  static constexpr int input_count() noexcept
  {
    return 0;
  }
  static constexpr int output_count() noexcept
  {
    return 0;
  }
  static constexpr int default_input_channel_count() noexcept
  {
    return 0;
  }
  static constexpr int default_output_channel_count() noexcept
  {
    return 0;
  }

  static bool input_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    return false;
  }

  static bool output_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    return false;
  }

  static constexpr int runtime_input_channel_count = 0;
  static constexpr int runtime_output_channel_count = 0;
};

template<avnd::bus_arg_processor T>
struct audio_bus_info<T>
{
  static constexpr int input_count() noexcept
  {
    return 1;
  }
  static constexpr int output_count() noexcept
  {
    return 1;
  }
  static constexpr int default_input_channel_count() noexcept
  {
    return avnd::input_channels<T>(2);
  }
  static constexpr int default_output_channel_count() noexcept
  {
    return avnd::output_channels<T>(2);
  }

  static bool input_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    static const constexpr int input_audio_busses = avnd::bus_introspection<T>::input_busses;
    if(index == 0)
    {
      info.id = index;
      copy_string(info.name, "Stereo In");

      info.channel_count = default_input_channel_count();
      info.channel_map = {};
      info.sample_size = avnd::polyphonic_arg_audio_effect<double, T> ? 64 : 32;
      info.is_main = true;
      info.is_cv = false;
      info.in_place = true;

      return true;
    }

    return false;
  }

  static bool output_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    static const constexpr int output_audio_busses = avnd::bus_introspection<T>::output_busses;

    if(index == 0)
    {
      info.id = index;
      copy_string(info.name, "Stereo Out");

      info.channel_count = default_output_channel_count();
      info.channel_map = {};
      info.sample_size = avnd::polyphonic_arg_audio_effect<double, T> ? 64 : 32;
      info.is_main = true;
      info.is_cv = false;
      info.in_place = true;

      return true;
    }

    return false;
  }

  int runtime_input_channel_count = default_input_channel_count();
  int runtime_output_channel_count = default_output_channel_count();
};

template<avnd::sample_arg_processor T>
struct audio_bus_info<T>
{
  static constexpr int input_count() noexcept
  {
    return 1;
  }
  static constexpr int output_count() noexcept
  {
    return 1;
  }
  static constexpr int default_input_channel_count() noexcept
  {
    return 1;
  }
  static constexpr int default_output_channel_count() noexcept
  {
    return 1;
  }

  static bool input_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    if(index == 0)
    {
      info.id = index;
      copy_string(info.name, "Mono In");

      info.channel_count = default_input_channel_count();
      info.channel_map = CLAP_CHMAP_MONO;
      info.sample_size = avnd::mono_per_sample_arg_processor<double, T> ? 64 : 32;
      info.is_main = true;
      info.is_cv = false;
      info.in_place = true;

      return true;
    }

    return false;
  }

  static bool output_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    if(index == 0) {
      info.id = index;
      copy_string(info.name, "Mono Out");

      info.channel_count = default_output_channel_count();
      info.channel_map = CLAP_CHMAP_MONO;
      info.sample_size = avnd::mono_per_sample_arg_processor<double, T> ? 64 : 32;
      info.is_main = true;
      info.is_cv = false;
      info.in_place = true;

      return true;
    }

    return false;
  }

  int runtime_input_channel_count = default_input_channel_count();
  int runtime_output_channel_count = default_output_channel_count();
};


template<avnd::channel_arg_processor T>
struct audio_bus_info<T>
{
  static constexpr int input_count() noexcept
  {
    return 1;
  }
  static constexpr int output_count() noexcept
  {
    return 1;
  }
  static constexpr int default_input_channel_count() noexcept
  {
    return 1;
  }
  static constexpr int default_output_channel_count() noexcept
  {
    return 1;
  }

  static bool input_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    if(index == 0)
    {
      info.id = index;
      copy_string(info.name, "Mono In");

      info.channel_count = default_input_channel_count();
      info.channel_map = CLAP_CHMAP_MONO;
      info.sample_size = avnd::monophonic_arg_audio_effect<double, T> ? 64 : 32;
      info.is_main = true;
      info.is_cv = false;
      info.in_place = true;

      return true;
    }

    return false;
  }

  static bool output_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    if(index == 0)
    {
      info.id = index;
      copy_string(info.name, "Mono Out");

      info.channel_count = default_output_channel_count();
      info.channel_map = CLAP_CHMAP_MONO;
      info.sample_size = avnd::monophonic_arg_audio_effect<double, T> ? 64 : 32;
      info.is_main = true;
      info.is_cv = false;
      info.in_place = true;

      return true;
    }

    return false;
  }

  int runtime_input_channel_count = default_input_channel_count();
  int runtime_output_channel_count = default_output_channel_count();
};



template<avnd::bus_port_processor T>
struct audio_bus_info<T>
{
  using input_refl = avnd::audio_bus_input_introspection<T>;
  using output_refl = avnd::audio_bus_output_introspection<T>;
  static constexpr int input_count() noexcept
  {
    return input_refl::size;
  }
  static constexpr int output_count() noexcept
  {
    return output_refl::size;
  }
  static constexpr int default_input_channel_count() noexcept
  {
    return avnd::input_channels<T>(2);
  }
  static constexpr int default_output_channel_count() noexcept
  {
    return avnd::output_channels<T>(2);
  }

  static bool input_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    if(index < input_refl::size)
    {
      info.id = index;
      input_refl::for_nth_mapped(index, [&] <std::size_t I, typename C> (const avnd::field_reflection<I, C>& port) {
        copy_string(info.name, C::name());
        info.sample_size = avnd::poly_array_sample_port<double, C> ? 64 : 32;
      });

      info.channel_count = default_input_channel_count();
      info.channel_map = {};
      info.is_main = index == 0;
      info.is_cv = false;
      info.in_place = true;

      return true;
    }

    return false;
  }

  static bool output_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    if(index < output_refl::size)
    {
      info.id = index;
      output_refl::for_nth_mapped(index, [&] <std::size_t I, typename C> (const avnd::field_reflection<I, C>& port) {
        copy_string(info.name, C::name());
        info.sample_size = avnd::poly_array_sample_port<double, C> ? 64 : 32;
      });

      info.channel_count = default_output_channel_count();
      info.channel_map = {};
      info.is_main = index == 0;
      info.is_cv = false;
      info.in_place = true;

      return true;
    }

    return false;
  }

  int runtime_input_channel_count = default_input_channel_count();
  int runtime_output_channel_count = default_output_channel_count();
};

template<avnd::sample_port_processor T>
struct audio_bus_info<T>
{
  using input_refl = avnd::audio_bus_input_introspection<T>;
  using output_refl = avnd::audio_bus_output_introspection<T>;
  static constexpr int input_count() noexcept
  {
    return input_refl::size;
  }
  static constexpr int output_count() noexcept
  {
    return output_refl::size;
  }
  static constexpr int default_input_channel_count() noexcept
  {
    return input_count();
  }
  static constexpr int default_output_channel_count() noexcept
  {
    return output_count();
  }

  static bool input_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    if(index < input_count()) {
      info.id = index;
      input_refl::for_nth_mapped(index, [&] <std::size_t I, typename C> (const avnd::field_reflection<I, C>& port) {
        copy_string(info.name, C::name());
        info.sample_size = avnd::audio_sample_port<double, C> ? 64 : 32;
      });

      info.channel_count = 1;
      info.channel_map = {};
      info.is_main = index == 0;
      info.is_cv = false;
      info.in_place = true;

      return true;
    }

    return false;
  }

  static bool output_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    if(index < output_count()) {
      info.id = index;
      output_refl::for_nth_mapped(index, [&] <std::size_t I, typename C> (const avnd::field_reflection<I, C>& port) {
        copy_string(info.name, C::name());
        info.sample_size = avnd::audio_sample_port<double, C> ? 64 : 32;
      });

      info.channel_count = 1;
      info.channel_map = {};
      info.is_main = index == 0;
      info.is_cv = false;
      info.in_place = true;
      return true;
    }

    return false;
  }


  int runtime_input_channel_count = default_input_channel_count();
  int runtime_output_channel_count = default_output_channel_count();
};

template<avnd::channel_port_processor T>
struct audio_bus_info<T>
{
  using input_refl = avnd::audio_bus_input_introspection<T>;
  using output_refl = avnd::audio_bus_output_introspection<T>;
  static constexpr int input_count() noexcept
  {
    return input_refl::size;
  }
  static constexpr int output_count() noexcept
  {
    return output_refl::size;
  }
  static constexpr int default_input_channel_count() noexcept
  {
    return input_count();
  }
  static constexpr int default_output_channel_count() noexcept
  {
    return output_count();
  }

  static bool input_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    if(index < input_count()) {
      info.id = index;
      input_refl::for_nth_mapped(index, [&] <std::size_t I, typename C> (const avnd::field_reflection<I, C>& port) {
        copy_string(info.name, C::name());
        info.sample_size = avnd::mono_array_sample_port<double, C> ? 64 : 32;
      });

      info.channel_count = 1;
      info.channel_map = {};
      info.is_main = index == 0;
      info.is_cv = false;
      info.in_place = true;

      return true;
    }

    return false;
  }


  static bool output_info(
      uint32_t index,
      clap_audio_port_info& info)
  {
    if(index < output_count()) {
      info.id = index;
      output_refl::for_nth_mapped(index, [&] <std::size_t I, typename C> (const avnd::field_reflection<I, C>& port) {
        copy_string(info.name, C::name());
        info.sample_size = avnd::mono_array_sample_port<double, C> ? 64 : 32;
      });

      info.channel_count = 1;
      info.channel_map = {};
      info.is_main = index == 0;
      info.is_cv = false;
      info.in_place = true;

      return true;
    }

    return false;
  }

  int runtime_input_channel_count = default_input_channel_count();
  int runtime_output_channel_count = default_output_channel_count();
};

}
