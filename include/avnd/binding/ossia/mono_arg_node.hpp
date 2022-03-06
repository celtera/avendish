#pragma once
#include <avnd/binding/ossia/node.hpp>
/*
namespace oscr
{
template <typename T>
struct arg_mono_audio_ports
{
  ossia::audio_inlet audio_in;
  ossia::audio_outlet audio_out;

  bool check_input_channels(avnd::effect_container<T>& impl, ossia::exec_state_facade st)
  {
    if (audio_out->channels() != audio_in->channels())
    {
      int new_channels = audio_in->channels();
      this->impl.init_channels(new_channels, new_channels);
      do_prepare(impl, st, new_channels, new_channels);
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
class safe_node_mono_arg_base : public safe_node_base<T, arg_mono_audio_ports<T>>
{
public:
  explicit safe_node_mono_arg_base(ossia::exec_state_facade st) noexcept
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
  arg_mono_audio_ports<T> arg_ports;
};

template <typename T>
  requires avnd::monophonic_arg_audio_effect<double, T>
class safe_node<T> final : public safe_node_mono_arg_base<T>
{
public:
  safe_node(ossia::exec_state_facade st)
      : safe_node_mono_arg_base<T>{st}
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
    double* ins = {};
    double* outs = {};
    // FIXME

    // Run
    this->impl.effect(ins, outs, frames);

    this->finish_run();
  }
};

template <typename T>
  requires avnd::monophonic_arg_audio_effect<float, T>
class safe_node<T> final : public safe_node_mono_arg_base<T>
{
public:
  safe_node(ossia::exec_state_facade st)
      : safe_node_mono_arg_base<T>{st}
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
    float* ins = {};
    float* outs = {};
    // FIXME

    // Run
    this->impl.effect(ins, outs, frames);

    // FIXME copy back from float buffer into audio outs

    this->finish_run();
  }
};
}*/
