#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>

#include <QProcess>
#include <QString>
#include <QStringList>

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
    return "Launch a shell command detached from the host. "
           "The script runs with the chosen interpreter; Custom runs the given "
           "command line, where %s stands for the script (appended if absent).";
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

  enum Interpreter
  {
    System,
    Bash,
    Zsh,
    Fish,
    Sh,
    Python,
    PowerShell,
    Cmd,
    Custom
  };

  struct job
  {
    std::string script;
    Interpreter interpreter{System};
    std::string custom;
  };

  //! The program and arguments running the script, or nothing to go
  //! through ::system (the system shell).
  static QStringList commandLine(const job& j)
  {
    const auto script = QString::fromStdString(j.script);
    switch(j.interpreter)
    {
      case System:
        return {};
      case Bash:
        return {"bash", "-c", script};
      case Zsh:
        return {"zsh", "-c", script};
      case Fish:
        return {"fish", "-c", script};
      case Sh:
        return {"sh", "-c", script};
      case Python:
        return {"python3", "-c", script};
      case PowerShell:
#if defined(_WIN32)
        return {"powershell", "-NoProfile", "-Command", script};
#else
        return {"pwsh", "-NoProfile", "-Command", script};
#endif
      case Cmd:
        return {"cmd", "/C", script};
      case Custom: {
        auto args = QProcess::splitCommand(QString::fromStdString(j.custom));
        if(args.isEmpty())
          return {};
        bool replaced = false;
        for(auto& a : args)
          if(a.contains(QStringLiteral("%s")))
          {
            a.replace(QStringLiteral("%s"), script);
            replaced = true;
          }
        if(!replaced)
          args.push_back(script);
        return args;
      }
    }
    return {};
  }

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

    halp::combobox_t<"Interpreter", Interpreter> interpreter;
    halp::lineedit<"Custom command", "/bin/bash -c %s"> custom;
  } inputs;

  struct
  {
  } outputs;

  void operator()()
  {
    worker.request(
        job{std::move(inputs.command.value), inputs.interpreter.value,
            inputs.custom.value});
  }

  struct worker
  {
    std::function<void(job)> request;
    static void work(job&& j)
    {
      auto cmd = commandLine(j);
      if(cmd.isEmpty())
      {
        std::thread{[code = std::move(j.script)] { ::system(code.c_str()); }}.detach();
        return;
      }
      const auto program = cmd.takeFirst();
      QProcess::startDetached(program, cmd);
    }
  } worker;
};
}
