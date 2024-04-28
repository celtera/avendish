#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <QCoreApplication>
#include <QProcess>

#include <functional>
#include <memory>
#include <string>

namespace examples
{
struct ProcessLauncher
{
  static consteval auto name() { return "Process launcher"; }
  static consteval auto c_name() { return "avnd_process"; }
  static consteval auto category() { return "Script"; }
  static consteval auto uuid() { return "ac3afbbb-cbe0-4559-ac14-51250024b458"; }

  // This tag is an indication that the node will have start / stop methods called when
  // the execution starts or stop. Only relevant in systems with such an ability,
  // such as ossia score
  enum
  {
    process_exec
  };

  struct
  {
    struct
    {
      static constexpr auto name() { return "Script"; }
      enum widget
      {
        lineedit
      };

      std::string value;
    } command;
  } inputs;

  struct
  {
  } outputs;

  void start() { worker.request(inputs.command.value, process); }
  void stop()
  {
    worker.request("", process);
    process.reset();
  }

  struct worker
  {
    std::function<void(std::string, std::shared_ptr<QProcess>)> request;
    static std::function<void(ProcessLauncher&)>
    work(std::string&& s, std::shared_ptr<QProcess> cur)
    {
      if(s.empty())
      {
        // Stop case
        if(cur)
        {
          QMetaObject::invokeMethod(qApp, [cur] {
			  cur->terminate();
	   });
        }
        return [](ProcessLauncher& self) { self.process.reset(); };
      }
      else
      {
        // Start case
        auto ptr = std::make_shared<QProcess>();
        auto cmds = QProcess::splitCommand(QString::fromStdString(s));
        if(cmds.size() > 0)
        {
          ptr->setProgram(cmds[0]);
        }
        if(cmds.size() > 1)
        {
          cmds.remove(0);
          ptr->setArguments(cmds);
        }

        // QProcess needs to run in an event-loop based thing
        QMetaObject::invokeMethod(qApp, [=] { ptr->start(); });

        return [ptr](ProcessLauncher& self) { self.process = ptr; };
      }
    }
  } worker;

  std::shared_ptr<QProcess> process;
};
}
