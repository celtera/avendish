#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#define avnd_meta(name, value) \
  static consteval auto name() { return value; }
