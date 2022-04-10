#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Quick helper macro
#define halp_flag(flag) enum { flag }
#define halp_flags(...) enum { __VA_ARGS__ }
#define halp_meta(name, value) static consteval auto name() { return value; }

