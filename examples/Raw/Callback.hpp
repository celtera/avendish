#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/callback.hpp>
#include <halp/callback.hpp>

#include <functional>
#include <iostream>
namespace examples
{

/**
 * In this example, whenever "work" is sent, the two callbacks will
 * be called if they've been set on the host side.
 *
 * On patching environments, one output will be created per such message.
 */
struct Callback
{
  static consteval auto name() { return "Callback"; }
  static consteval auto c_name() { return "avnd_callback"; }
  static consteval auto uuid() { return "38b72efd-33f7-4272-a15f-36c7d83b9165"; }

  struct
  {
    struct
    {
      static consteval auto name() { return "a"; }
      float value;
    } a;
  } inputs;

  struct
  {
    struct
    {
      static consteval auto name() { return "work"; }
      static consteval auto func() { return &Callback::work; }
    } member;
  } messages;

  struct
  {
    // Here we use a simple helper type to not bloat the code too much.
    struct
    {
      static consteval auto name() { return "bang"; }
      halp::basic_callback<void(float)> call;
      static_assert(avnd::function_view_ish<decltype(call)>);
    } bang;

    // Using std::function or similar should also work without issues:
    // What matters is that the concept halp::callback is respected.
    // Basically anything that looks like std::function will work
    struct
    {
      static consteval auto name() { return "bong"; }
      std::function<void(float)> call;
      static_assert(avnd::function_ish<decltype(call)>);
    } bong;

    // TODO: std::function_ref.
  } outputs;

  void work()
  {
    if(outputs.bang.call)
      outputs.bang.call(inputs.a.value * 10);

    if(outputs.bong.call)
      outputs.bong.call(inputs.a.value * 1000);
  }
};
}
