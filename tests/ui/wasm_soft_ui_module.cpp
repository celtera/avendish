/* SPDX-License-Identifier: GPL-3.0-or-later */

// Emscripten validation module for the soft-framebuffer UI shell: exposes
// the ClapUiPlug test plug-in's UI through avnd::soft_ui::bind_wasm_ui.
// Driven headlessly under node by wasm_soft_ui_test.mjs (frame determinism,
// pointer interaction, bus round trip through the synchronous transport).

#include <ClapUiPlug.hpp>

#include <avnd/binding/ui/soft/implementation.hpp>

#include <avnd/binding/ui/soft/wasm.hpp>

EMSCRIPTEN_BINDINGS(avnd_soft_ui_test)
{
  avnd::soft_ui::bind_wasm_ui<avnd_test::ClapUiPlug>("SoftUI");
}
