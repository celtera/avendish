#pragma once
#if 0
#include <avnd/binding/ossia/node.hpp>

namespace oscr
{
template <typename T>
struct arg_poly_audio_ports
{
  ossia::audio_inlet audio_in;
  ossia::audio_outlet audio_out;

  int last_input_channels{};
  int last_output_channels{};
  int actual_input_channels{};
  int actual_output_channels{};

  bool check_input_channels(avnd::effect_container<T>& impl, ossia::exec_state_facade st)
  {
    // What we got on our input
    int input_channels = audio_in.data.channels();
    int output_channels = audio_out.data.channels();

    // If the inputs changed we have to reconfigure
    if (input_channels != last_input_channels)
    {
      int wanted_input_channels = avnd::input_channels<T>(-1);
      int wanted_output_channels = avnd::output_channels<T>(-1);

      int effective_input_channels = -1;
      int effective_output_channels = -1;

      // Compute how many actual input channels we are going to work with
      if (wanted_input_channels == -1)
      {
        // the processor can handle any amount of input
        effective_input_channels = input_channels;
      }
      else
      {
        if (input_channels != wanted_input_channels)
        {
          if (input_channels > wanted_input_channels)
          {
            // we have more inputs than the processor can handle
            effective_input_channels = wanted_input_channels;
          }
          else // if(actual_input_channels < wanted_input_channels)
          {
            // processor wants two inputs but we have only one... for now don't run
            // TODO: add zeros instead?
            return false;
          }
        }
        else
        {
          // we have exactly as many inputs as the processor can handle
          effective_input_channels = wanted_input_channels;
        }
      }

      // Compute how many actual input channels we are going to work with
      if (wanted_output_channels == -1)
      {
        // Unspecified: we assume as many as there are input channels
        effective_output_channels = effective_input_channels;
      }
      else
      {
        effective_output_channels = wanted_output_channels;
      }

      // Reallocate our audio output port's buffers
      if (output_channels != effective_output_channels)
      {
        audio_out.data.set_channels(effective_output_channels);
      }

      // Reinitialize internal effects buffers since it is going to get different amount of buffers
      if (effective_input_channels != actual_input_channels
          || effective_output_channels != actual_output_channels)
      {
        actual_input_channels = effective_input_channels;
        actual_output_channels = effective_output_channels;

        do_prepare(impl, st, actual_input_channels, actual_output_channels);
      }

      last_input_channels = effective_input_channels;
      last_output_channels = effective_output_channels;
    }
    for (auto& chan : audio_in.data)
    {
      chan.resize(st.bufferSize());
    }
    for (auto& chan : audio_out.data)
    {
      chan.resize(st.bufferSize());
    }

    return true;
  }
};

template <typename T>
class safe_node_poly_arg_base : public safe_node_base<T>
{
public:
  explicit safe_node_poly_arg_base(ossia::exec_state_facade st) noexcept
  {
    // Initialize our own buffers
    // By default we'll assume 2 input channels
    this->m_inlets.push_back(&arg_ports.audio_in);
    arg_ports.audio_in->set_channels(2);

    this->m_outlets.push_back(&arg_ports.audio_out);
    arg_ports.audio_out->set_channels(2);

    this->impl.init_channels(2, 2);
    arg_ports.check_input_channels(this->impl, st);

    this->finish_init();
  }

protected:
  arg_poly_audio_ports<T> arg_ports;
};

template <typename T>
  requires avnd::polyphonic_arg_audio_effect<double, T>
class safe_node<T> final : public safe_node_poly_arg_base<T>
{
public:
  safe_node(ossia::exec_state_facade st)
      : safe_node_poly_arg_base<T>{st}
  {
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    // Count how many input channels we have / need, potentially allocate more space:
    if (!this->arg_ports.check_input_channels(this->impl, st))
      return;

    if (!this->prepare_run())
      return;

    // Initialize audio ports
    auto [start, frames] = st.timings(tk);
    double** ins = {};
    double** outs = {};
    if (this->arg_ports.actual_input_channels > 0)
    {
      ins = (double**)alloca(sizeof(double*) * this->arg_ports.actual_input_channels);
      for (int i = 0; i < this->arg_ports.actual_input_channels; i++)
      {
        ins[i] = this->arg_ports.audio_in.data.channel(i).data() + start;
      }
    }

    if (this->arg_ports.actual_output_channels > 0)
    {
      outs = (double**)alloca(sizeof(double*) * this->arg_ports.actual_output_channels);
      for (int i = 0; i < this->arg_ports.actual_output_channels; i++)
      {
        outs[i] = this->arg_ports.audio_out.data.channel(i).data() + start;
      }
    }

    // Run
    this->impl.effect(ins, outs, frames);

    this->finish_run();
  }
};

template <typename T>
  requires avnd::polyphonic_arg_audio_effect<float, T>
class safe_node<T> final : public safe_node_poly_arg_base<T>
{
public:
  safe_node(ossia::exec_state_facade st)
      : safe_node_poly_arg_base<T>{st}
  {
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    // Count how many input channels we have / need, potentially allocate more space:
    if (!this->arg_ports.check_input_channels(this->impl, st))
      return;

    if (!this->prepare_run())
      return;

    // Initialize audio ports
    auto [start, frames] = st.timings(tk);
    float** ins = {};
    float** outs = {};
    if (this->arg_ports.actual_input_channels > 0)
    {
      ins = (float**)alloca(sizeof(float*) * this->arg_ports.actual_input_channels);
      for (int i = 0; i < this->arg_ports.actual_input_channels; i++)
      {
        // FIXME allocate and copy in float buffer
        ins[i] = this->arg_ports.audio_in.data.channel(i).data() + start;
      }
    }

    if (this->arg_ports.actual_output_channels > 0)
    {
      outs = (float**)alloca(sizeof(float*) * this->arg_ports.actual_output_channels);
      for (int i = 0; i < this->arg_ports.actual_output_channels; i++)
      {
        // FIXME allocate float buffer
        outs[i] = this->arg_ports.audio_out.data.channel(i).data() + start;
      }
    }

    // Run
    this->impl.effect(ins, outs, frames);

    // FIXME copy back from float buffer into audio outs

    this->finish_run();
  }
};
}
#endif
