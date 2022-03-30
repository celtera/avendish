#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#define halp_meta(name, value) \
  static consteval auto name() { return value; }
