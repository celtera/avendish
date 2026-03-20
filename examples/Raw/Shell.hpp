#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <functional>
#include <string>
#include <thread>
namespace examples
{
struct Shell
{
  static consteval auto name() { return "Shell command"; }
  static consteval auto c_name() { return "avnd_shell"; }
  static consteval auto category() { return "Script"; }
  static consteval auto uuid() { return "7e4ae744-1825-4f1c-9fc9-675e41f316bc"; }
  static consteval auto description()
  {
    return "Launch a shell command detached from the host";
  }
  static consteval auto manual_url()
  {
    return "https://ossia.io/score-docs/processes/process-launcher.html#shell-command";
  }
  static consteval auto author() { return "Jean-Michaël Celerier"; }

  // This tag is an indication that the operator() should only called on
  // the first tick, not on every tick
  enum
  {
    single_exec
  };

  struct
  {
    struct
    {
      static constexpr auto name() { return "Script"; }
      static constexpr auto language() { return "shell"; }
      enum widget
      {
        textedit
      };

      struct range
      {
        const std::string_view init = "#!/bin/bash\n";
      };
      std::string value;
    } command;
  } inputs;

  struct
  {
  } outputs;

  void operator()() { worker.request(std::move(inputs.command.value)); }

  struct worker
  {
    std::function<void(std::string)> request;
    static void work(std::string&& s)
    {
      std::thread{[code = std::move(s)] { ::system(code.c_str()); }}.detach();
    }
  } worker;
};
}
