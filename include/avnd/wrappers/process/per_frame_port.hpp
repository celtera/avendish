#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/process/base.hpp>
#include <boost/container/small_vector.hpp>

namespace avnd
{

template <typename FP, typename T>
struct input_frame_storage
{
  static constexpr auto input_frame_storage_count = 0;
  void allocate_buffers(process_setup setup, auto&& f) { }
};

template <typename FP, typename T>
  requires(frame_input_port_count<FP, T> != 0)
struct input_frame_storage<FP, T>
{
  static constexpr auto input_frame_storage_count = frame_input_port_count<FP, T>;
  boost::container::small_vector<FP, 8 * input_frame_storage_count>
      m_input_frame_storage;

  void allocate_buffers(process_setup setup, auto&& f)
  {
    // For dynamic frame ports, we need a small buffer to hold one sample
    // per channel. The total across all dynamic input (resp. output) frame ports
    // cannot exceed setup.input_channels (resp. setup.output_channels).
    m_input_frame_storage.resize(setup.input_channels);
  }
};

template <typename FP, typename T>
struct output_frame_storage
{
  static constexpr auto output_frame_storage_count = 0;
  void allocate_buffers(process_setup setup, auto&& f) { }
};

template <typename FP, typename T>
  requires(frame_output_port_count<FP, T> != 0)
struct output_frame_storage<FP, T>
{
  static constexpr auto output_frame_storage_count = frame_input_port_count<FP, T>;
  boost::container::small_vector<FP, 8 * output_frame_storage_count>
      m_output_frame_storage;

  void allocate_buffers(process_setup setup, auto&& f)
  {
    m_output_frame_storage.resize(setup.output_channels);
  }
};

/**
 * Handles case where inputs / outputs are per-frame multichannel ports:
 *  - fixed:   struct { float frame[N]; static constexpr int channels() { return N; } };
 *  - dynamic: struct { float* frame; int channels; };
 *
 * For fixed frame ports, frame storage is inline in the struct.
 * For dynamic frame ports, we allocate a small buffer (one sample per channel)
 * and assign the frame pointer before processing.
 */
template <typename T>
  requires(per_frame_port_processor<float, T> || per_frame_port_processor<double, T>)
struct process_adapter<T>
    : input_frame_storage<
          std::conditional_t<per_frame_port_processor<double, T>, double, float>, T>
    , output_frame_storage<
          std::conditional_t<per_frame_port_processor<double, T>, double, float>, T>
{
  // The native FP type of the frame ports
  using frame_fp_type
      = std::conditional_t<per_frame_port_processor<double, T>, double, float>;

  void process_frame(T& fx, auto& ins, auto& outs, auto&& tick)
  {
    if constexpr(requires { fx(ins, outs, tick); })
      return fx(ins, outs, tick);
    else if constexpr(requires { fx(ins, tick); })
      return fx(ins, tick);
    else if constexpr(requires { fx(outs, tick); })
      return fx(outs, tick);
    else if constexpr(requires { fx(tick); })
      return fx(tick);
    else
      static_assert(std::is_void_v<T>, "Canno call processor");
  }

  void process_frame(T& fx, auto& ins, auto& outs)
  {
    if constexpr(requires { fx(ins, outs); })
      return fx(ins, outs);
    else if constexpr(requires { fx(ins); })
      return fx(ins);
    else if constexpr(requires { fx(outs); })
      return fx(outs);
    else if constexpr(requires { fx(); })
      return fx();
    else
      static_assert(std::is_void_v<T>, "Canno call processor");
  }

  void allocate_buffers(process_setup setup, auto&& f)
  {
    input_frame_storage<frame_fp_type, T>::allocate_buffers(setup, f);
    output_frame_storage<frame_fp_type, T>::allocate_buffers(setup, f);
  }

  template <std::floating_point FP>
  void process(
      avnd::effect_container<T>& implementation, avnd::span<FP*> in, avnd::span<FP*> out,
      const auto& tick, auto&&... params)
  {
    auto& fx = implementation.effect;
    auto& ins = implementation.inputs();
    auto& outs = implementation.outputs();

    // Assign frame pointers for dynamic frame ports.
    // Fixed frame ports already have inline arrays so they are skipped.
    if constexpr(input_frame_storage<frame_fp_type, T>::input_frame_storage_count > 0)
    {
      int offset = 0;
      avnd::for_each_field_ref(ins, [&]<typename Field>(Field& field) {
        if constexpr(
            avnd::generic_audio_frame_port<Field>
            && avnd::dynamic_poly_audio_port<Field>)
        {
          const int ch = avnd::get_channels(field);
          if(offset + ch <= (int)this->m_input_frame_storage.size())
            field.frame = this->m_input_frame_storage.data() + offset;
          offset += ch;
        }
      });
    }

    if constexpr(output_frame_storage<frame_fp_type, T>::output_frame_storage_count > 0)
    {
      int offset = 0;
      avnd::for_each_field_ref(outs, [&]<typename Field>(Field& field) {
        if constexpr(
            avnd::generic_audio_frame_port<Field>
            && avnd::dynamic_poly_audio_port<Field>)
        {
          const int ch = avnd::get_channels(field);
          if(offset + ch <= (int)this->m_output_frame_storage.size())
            field.frame = this->m_output_frame_storage.data() + offset;
          offset += ch;
        }
      });
    }

    const auto n = get_frames(tick);
    for(int32_t i = 0; i < n; i++)
    {
      // Copy inputs in the effect
      {
        int k = 0;
        // Here we know that we have a single effect. We copy the sample data directly inside.
        avnd::for_each_field_ref(ins, [&k, in, i]<typename Field>(Field& field) {
          if constexpr(avnd::generic_audio_frame_port<Field>)
          {
            if(k < in.size())
            {
              for(int channel = 0; channel < avnd::get_channels(field); channel++, k++)
              {
                field.frame[channel] = in[k][i];
              }
            }
          }
        });
      }

      // Process the various parameters
      process_smooth(implementation, params...);

      // Process
      if constexpr(avnd::has_tick<T>)
      {
        process_frame(fx, ins, outs, current_tick(implementation));
      }
      else
      {
        process_frame(fx, ins, outs);
      }

      // Copy the outputs
      {
        int k = 0;
        // Here we know that we have a single effect. We copy the sample data directly inside.
        avnd::for_each_field_ref(outs, [&k, out, i]<typename Field>(Field& field) {
          if constexpr(avnd::generic_audio_frame_port<Field>)
          {
            if(k < out.size())
            {
              for(int channel = 0; channel < avnd::get_channels(field); channel++, k++)
              {
                out[k][i] = field.frame[channel];
              }
            }
          }
        });
      }
    }
  }
};

}
