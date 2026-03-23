#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/configure.hpp>
#include <halp/log.hpp>

namespace godot_binding
{
struct config
{
  using logger_type = halp::basic_logger;
};
}
