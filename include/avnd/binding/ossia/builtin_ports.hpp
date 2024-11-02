#pragma once
#include <avnd/concepts/processor.hpp>
#include <avnd/introspection/messages.hpp>
#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/value_port.hpp>

namespace oscr
{

template <typename T>
struct builtin_arg_audio_ports
{
  void init(ossia::inlets& inlets, ossia::outlets& outlets) { }
};

template <avnd::audio_argument_processor T>
struct builtin_arg_audio_ports<T>
{
  ossia::audio_inlet in;
  ossia::audio_outlet out;

  void init(ossia::inlets& inlets, ossia::outlets& outlets)
  {
    inlets.push_back(&in);
    outlets.push_back(&out);
  }
};

template <typename T>
struct builtin_arg_value_ports
{
  void init(ossia::inlets& inlets, ossia::outlets& outlets) { }
};

template <avnd::tag_cv T>
struct builtin_arg_value_ports<T>
{
  ossia::value_inlet in;
  ossia::value_outlet out;

  void init(ossia::inlets& inlets, ossia::outlets& outlets)
  {
    inlets.push_back(&in);
    outlets.push_back(&out);
  }
};

template <typename T>
struct builtin_message_value_ports
{
  void init(ossia::inlets& inlets) { }
};

template <typename T>
  requires(avnd::messages_introspection<T>::size > 0)
struct builtin_message_value_ports<T>
{
  ossia::value_inlet message_inlets[avnd::messages_introspection<T>::size];
  void init(ossia::inlets& inlets)
  {
    for(auto& in : message_inlets)
    {
      inlets.push_back(&in);
    }
  }
};

}
