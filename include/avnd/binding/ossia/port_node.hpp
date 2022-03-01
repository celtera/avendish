#pragma once
#include <avnd/binding/ossia/node.hpp>
#include <avnd/wrappers/audio_channel_manager.hpp>

namespace oscr
{
template <typename T>
class safe_node : public safe_node_base<T>
{
//  [[no_unique_address]] avnd::audio_channel_manager<T> channels;

public:
  explicit safe_node(ossia::exec_state_facade st) noexcept
  {
  }
};
/*
template <typename T>
requires avnd::port_arg_audio_effect<double, T>
class safe_node<T> final
    : public safe_node_port_base<T>
{
  public:
    safe_node(ossia::exec_state_facade st)
        : safe_node_port_base<T>{st}
    {

    }

    void
    run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
    {
      // Count how many input channels we have / need, potentially allocate more space:
      if(!this->arg_ports.check_input_channels(this->impl, st))
        return;

      if(!this->prepare_run())
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
class safe_node<T> final
  : public safe_node_port_base<T>
{
  public:
    safe_node(ossia::exec_state_facade st)
        : safe_node_port_base<T>{st}
    {

    }

    void
    run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
    {
      // Count how many input channels we have / need, potentially allocate more space:
      if(!this->arg_ports.check_input_channels(this->impl, st))
        return;

      if(!this->prepare_run())
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
};*/
}
