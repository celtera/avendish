#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Main TouchDesigner binding header
 * Include this file to use Avendish with TouchDesigner
 */

#include <avnd/binding/touchdesigner/configure.hpp>
#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/binding/touchdesigner/parameter_setup.hpp>

// CHOP bindings
#include <avnd/binding/touchdesigner/chop/audio_processor.hpp>

// TODO: TOP, SOP, DAT bindings will be added here in future phases

namespace avnd::touchdesigner
{
  // Re-export main namespace for convenience
  using namespace ::touchdesigner;
}
