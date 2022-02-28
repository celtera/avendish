#pragma once
#include <avnd/concepts/processor.hpp>
#include <avnd/concepts/audio_processor.hpp>
#include <avnd/wrappers/channels_introspection.hpp>
#include <avnd/wrappers/effect_container.hpp>

namespace avnd
{
/**
 * TODO:
 * - support effects with 1 inputs / N outputs
 *
 * Technically, nothing would prevent from instantiating them N times.
 */

template<typename T>
struct audio_channel_manager;

/**
 * Possible cases:
 * - float operator()(float in);
 * - void operator()(float* in, float* out);
 * - only one input and output port (sample or channel)
 *
 * Channels are always: output == input as the processor
 * is mono by definition.
 */
template<typename T>
requires avnd::monophonic_audio_processor<T>
struct audio_channel_manager<T>
{
    explicit audio_channel_manager(auto& processor) { }

    bool set_input_channels(auto& processor, int input_id, int channels)
    {
      if(input_id != 0)
        return false;
      actual_runtime_inputs = std::max(1, channels);
      actual_runtime_outputs = actual_runtime_inputs;
      return true;
    }

    bool set_output_channels(auto& processor, int output_id, int channels)
    {
      if(output_id != 0)
        return false;
      actual_runtime_inputs = std::max(1, channels);
      actual_runtime_outputs = actual_runtime_inputs;
      return true;
    }

    int get_input_channels(auto& processor, int input_id)
    {
      if(input_id != 0)
        return 0;
      return 1;
    }

    int get_output_channels(auto& processor, int output_id)
    {
      if(output_id != 0)
        return 0;
      return 1;
    }

    int actual_runtime_inputs = 0;
    int actual_runtime_outputs = 0;
};

/**
 *  Modular-like with multiple CV inputs / outputs.
 */
template<typename T>
requires avnd::poly_per_sample_port_processor<float, T>
      || avnd::poly_per_sample_port_processor<double, T>
struct audio_channel_manager<T>
{
    static constexpr const int detected_input_channels = avnd::input_channels_introspection<T>::input_channels;
    static constexpr const int detected_output_channels = avnd::output_channels_introspection<T>::output_channels;

    explicit audio_channel_manager(auto& processor) { }

    bool set_input_channels(auto& processor, int input_id, int channels)
    {
      // Every channel here is necessarily mono.
      return channels == 1;
    }

    bool set_output_channels(auto& processor, int output_id, int channels)
    {
      return channels == 1;
    }

    int get_input_channels(auto& processor, int input_id)
    {
      return 1;
    }

    int get_output_channels(auto& processor, int output_id)
    {
      return 1;
    }

    static constexpr const int actual_runtime_inputs = detected_input_channels;
    static constexpr const int actual_runtime_outputs = detected_output_channels;
};

/**
 * Case void operator()(float** in, int n_in, float** out, int n_out);
 */
template<typename T>
requires avnd::bus_arg_processor<T>
struct audio_channel_manager<T>
{
    static constexpr const int detected_input_channels = avnd::input_channels_introspection<T>::input_channels;
    static constexpr const int detected_output_channels = avnd::output_channels_introspection<T>::output_channels;

    explicit audio_channel_manager(auto& processor) { }

    bool set_input_channels(auto& processor, int input_id, int channels)
    {
      if(input_id != 0)
        return false;
      if constexpr(detected_input_channels == -1)
      {
        // The effect is open to any number of channel
        actual_runtime_inputs = channels;

        // Maybe the output isn't fixed, if so take the input
        if constexpr(detected_output_channels == -1)
          actual_runtime_outputs = actual_runtime_inputs;

        return true;
      }
      else
      {
        // The effect has a fixed number of inputs
        actual_runtime_inputs = detected_input_channels;

        // Maybe the output isn't fixed, if so take the input
        if constexpr(detected_output_channels == -1)
          actual_runtime_outputs = actual_runtime_inputs;

        return (channels == detected_input_channels);
      }
      return true;
    }

    bool set_output_channels(auto& processor, int output_id, int channels)
    {
      if(output_id != 0)
        return false;
      if constexpr(detected_output_channels == -1)
      {
        // Output isn't fixed
        actual_runtime_outputs = channels;
      }
      else
      {
        // Output is fixed, enforce the fixed one
        actual_runtime_outputs = detected_output_channels;
        return (channels == detected_output_channels);
      }
      return true;
    }

    int get_input_channels(auto& processor, int input_id)
    {
      if(input_id == 0)
        return actual_runtime_inputs;
      return 0;
    }

    int get_output_channels(auto& processor, int output_id)
    {
      if(output_id == 0)
        return actual_runtime_outputs;
      return 0;
    }

    int actual_runtime_inputs = std::max(detected_input_channels, 0);
    int actual_runtime_outputs = std::max(detected_output_channels, 0);
};

template<typename T>
requires (avnd::poly_array_port_based<float, T> || avnd::poly_array_port_based<double, T>)
struct audio_channel_manager<T>
{
    using in_refl = avnd::audio_bus_input_introspection<T>;
    using out_refl = avnd::audio_bus_output_introspection<T>;
    // Given the processor, and the current inputs,
    // tells us how many channels / instances of the plug-in must
    // actually be allocated for each port.

    explicit audio_channel_manager(avnd::effect_container<T>& eff)
    {
      auto& processor = eff.effect;
      // Initialize the local array with the default values
      int i = 0;
      auto& inputs = avnd::get_inputs(processor);
      in_refl::for_all(inputs, [this, &i] <avnd::audio_port P> (P& p) {
        if constexpr(avnd::fixed_poly_audio_port<P>) {
          this->input_channels[i] = p.channels();
        } else {
          this->input_channels[i] = p.channels;
        }
        i++;
      });

      i = 0;
      auto& outputs = avnd::get_outputs(processor);
      out_refl::for_all(outputs, [this, &i] <avnd::audio_port P> (P& p) {
        if constexpr(avnd::fixed_poly_audio_port<P>) {
          this->output_channels[i] = p.channels();
        } else {
          this->output_channels[i] = p.channels;
        }
        i++;
      });
    }

    // By convention, if the processor has audio passed as arguments / return
    // from its operator() function, they count as a virtual first port.
    bool set_input_channels(avnd::effect_container<T>& processor, int input_id, int channels)
    {
      std::optional<int> ok{};
      auto& inputs = avnd::get_inputs(processor);
      in_refl::for_nth_mapped(
            inputs, input_id,
            [channels, &ok] <avnd::audio_port P> (P& bus) -> void {
        if constexpr(requires { bus.channels = channels; }) {
         bus.channels = channels;
         ok = bus.channels;
        }
        else if constexpr(requires { P::channels(); }) {
          if(P::channels() == channels)
           ok = P::channels();
          else
           ok = std::nullopt;
        }
        else {
          AVND_ERROR(decltype(bus), "Should not happen");
           ok = std::nullopt;
        }
      });

      if(ok)
      {
        this->input_channels[input_id] = *ok;

        // We may have to check our outputs to see if any need updating
        update_outputs_from_input(processor, input_id, *ok);
      }

      return bool(ok);
    }

    void update_outputs_from_input(avnd::effect_container<T>& processor, int changed_input_id, int new_input_channel_count)
    {
      if constexpr(out_refl::size != 0)
      {
        // First check if we should match the first audio output to the first audio input
        auto& outputs = avnd::get_outputs(processor);
        auto& first = out_refl::template get<0>(outputs);
        using first_type = std::decay_t<decltype(first)>;
        if constexpr(avnd::dynamic_poly_audio_port<first_type>)
        {
          first.channels = new_input_channel_count;
          this->output_channels[0] = new_input_channel_count;
        }

        if(out_refl::size > 1)
        {
          // Then check all audio ports which may have channel mappings corresponding to the input channel.
          auto& inputs = avnd::get_inputs(processor);
          int i = 0;
          out_refl::for_all(outputs, [this, &inputs, &i] (auto& out) {
            if constexpr(requires { out.mimick_channel; })
            {
              auto& mimicked_port = (inputs.*out.mimick_channel);
              if constexpr(requires { mimicked_port.channels(); })
              {
                out.channels = mimicked_port.channels();
                this->output_channels[i] = out.channels;
              }
              else if constexpr(requires { mimicked_port.channels; })
              {
                out.channels = mimicked_port.channels;
                this->output_channels[i] = out.channels;
              }
            }
            i++;
          });
        }
      }
      else
      {
        return;
      }
    }

    bool set_output_channels(avnd::effect_container<T>& processor, int output_id, int channels)
    {
      std::optional<int> ok{};
      auto& inputs = avnd::get_inputs(processor);
      auto& outputs = avnd::get_outputs(processor);
      out_refl::for_nth_mapped(
            outputs, output_id,
            [channels, &inputs, &ok] <avnd::audio_port P>(P& bus) -> void {
        if constexpr(requires { bus.mimick_channel; })
        {
           int matching_input_channels{};

           auto& mimicked_port = (inputs.*bus.mimick_channel);
           if constexpr(requires { mimicked_port.channels(); })
             matching_input_channels = mimicked_port.channels();
           else if constexpr(requires { mimicked_port.channels; })
             matching_input_channels = mimicked_port.channels;

           bus.channels = matching_input_channels;

           if(channels == matching_input_channels)
             ok = channels;
           else
             ok = std::nullopt;
        }
        else if constexpr(requires { bus.channels = channels; }) {
         bus.channels = channels;
         ok = bus.channels;
        }
        else if constexpr(requires { P::channels(); }) {
          if(P::channels() == channels)
           ok = P::channels();
          else
           ok = std::nullopt;
        }
        else {
          AVND_ERROR(decltype(bus), "Should not happen");
           ok = std::nullopt;
        }
      });

      if(ok)
      {
        this->output_channels[output_id] = *ok;
      }
      return bool(ok);
    }

    int get_input_channels(T& processor, int input_id)
    {
      return input_channels[input_id];
    }

    int get_output_channels(T& processor, int output_id)
    {
      return output_channels[output_id];
    }

    // One of the two float/double cases will be null necessarily
    static constexpr int input_bus_count =
        avnd::poly_sample_array_input_port_count<float, T>
    + avnd::poly_sample_array_input_port_count<double, T>;

    static constexpr int output_bus_count =
        avnd::poly_sample_array_input_port_count<float, T>
    + avnd::poly_sample_array_input_port_count<double, T>;

    [[no_unique_address]]
    std::array<int, input_bus_count> input_channels;

    [[no_unique_address]]
    std::array<int, output_bus_count> output_channels;

    int actual_runtime_inputs = 0;
    int actual_runtime_outputs = 0;
};

// Case for everything that does not handle audio
template<typename T>
requires (!avnd::float_processor<T> && !avnd::double_processor<T>)
struct audio_channel_manager<T>
{
  static constexpr const int detected_input_channels = avnd::input_channels_introspection<T>::input_channels;
  static constexpr const int detected_output_channels = avnd::output_channels_introspection<T>::output_channels;
  static_assert(detected_input_channels <= 0);
  static_assert(detected_output_channels <= 0);

  explicit audio_channel_manager(auto& processor) { }

  bool set_input_channels(auto& processor, int input_id, int channels)
  {
    return channels == 0;
  }

  bool set_output_channels(auto& processor, int input_id, int channels)
  {
    return channels == 0;
  }

  int get_input_channels(auto& processor, int input_id)
  {
    return 0;
  }

  int get_output_channels(auto& processor, int output_id)
  {
    return 0;
  }

  static constexpr int actual_runtime_inputs = 0;
  static constexpr int actual_runtime_outputs = 0;
};
/*
template<typename T>
requires (avnd::poly_array_port_based<float, T> || avnd::poly_array_port_based<double, T>)
struct audio_channel_manager<T>
{

};
*/
}
