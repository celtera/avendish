/* SPDX-License-Identifier: GPL-3.0-or-later */

// End-to-end VST2 editor test: loads the ClapUiPlug plug-in built with the
// vintage binding, opens its editor in a real parent window, drives it with
// injected mouse messages, and verifies parameter values, the automation
// gesture callbacks (audioMaster BeginEdit/Automate/EndEdit) and the
// message-bus round trip. Runs on Windows (Win32 blit) and Linux (X11 blit;
// the editor makes progress through effEditIdle, like a real VST2 host).

#include <catch2/catch_test_macros.hpp>

#if (defined(_WIN32) || defined(__linux__)) && defined(AVND_TEST_VST2_PATH)

#include "gui_test_host.hpp"

#include <avnd/binding/vintage/vintage.hpp>

#if !defined(_WIN32)
#include <dlfcn.h>
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#define AVND_TEST_DLOPEN(path) (void*)LoadLibraryA(path)
#define AVND_TEST_DLSYM(lib, name) (void*)GetProcAddress((HMODULE)lib, name)
#define AVND_TEST_DLCLOSE(lib) FreeLibrary((HMODULE)lib)
#else
#define AVND_TEST_DLOPEN(path) dlopen(path, RTLD_NOW | RTLD_LOCAL)
#define AVND_TEST_DLSYM(lib, name) dlsym(lib, name)
#define AVND_TEST_DLCLOSE(lib) dlclose(lib)
#endif

namespace
{
struct host_log
{
  int begin_edits{}, end_edits{}, automates{};
  int last_automated_index{-1};
  float last_automated_value{-1.f};
};
static host_log g_host;

static intptr_t host_callback(
    vintage::Effect* effect, int32_t opcode, int32_t index, intptr_t value,
    void* ptr, float opt)
{
  using O = vintage::HostOpcodes;
  switch(static_cast<O>(opcode))
  {
    case O::Automate:
      g_host.automates++;
      g_host.last_automated_index = index;
      g_host.last_automated_value = opt;
      return 1;
    case O::BeginEdit:
      g_host.begin_edits++;
      return 1;
    case O::EndEdit:
      g_host.end_edits++;
      return 1;
    case O::GetSampleRate:
      return 48000;
    case O::GetBlockSize:
      return 512;
    case O::Version:
      return 2400;
    default:
      return 0;
  }
}
}

TEST_CASE("vst2 editor: embed, interact, gestures, bus", "[vintage_gui]")
{
  using vintage::EffectOpcodes;
  using namespace avnd_test_gui;

  void* lib = AVND_TEST_DLOPEN(AVND_TEST_VST2_PATH);
  REQUIRE(lib);
  using entry_t = vintage::Effect* (*)(vintage::HostCallback);
  const auto entry = (entry_t)AVND_TEST_DLSYM(lib, "VSTPluginMain");
  REQUIRE(entry);

  vintage::Effect* fx = entry(&host_callback);
  REQUIRE(fx);
  REQUIRE(((int32_t)fx->flags & (int32_t)vintage::EffectFlags::HasEditor) != 0);
  REQUIRE(fx->numParams == 2);

  const auto dispatch = [fx](EffectOpcodes op, int32_t index = 0,
                             intptr_t value = 0, void* ptr = nullptr,
                             float opt = 0.f) {
    return fx->dispatcher(fx, (int32_t)op, index, value, ptr, opt);
  };

  dispatch(EffectOpcodes::Open);

  // A VST2 host clocks the editor with effEditIdle.
  const auto tick = [&] { dispatch(EffectOpcodes::EditIdle); };

  // Editor rect must be available before opening
  vintage::Rect* rect{};
  REQUIRE(dispatch(EffectOpcodes::EditGetRect, 0, 0, &rect) == 1);
  REQUIRE(rect);
  CHECK(rect->right == 320);
  CHECK(rect->bottom == 220);

  const native_window parent
      = create_parent(rect->right, rect->bottom, L"avnd_vintage_gui_parent");
  REQUIRE(parent);
  REQUIRE(dispatch(EffectOpcodes::EditOpen, 0, 0, (void*)(uintptr_t)parent) == 1);

  pump_messages(300, tick);

  const native_window child = find_child(parent);
  REQUIRE(child);

  // "Level" starts at 0.25 (param 0, range 0..1 so normalized == value)
  CHECK(fx->getParameter(fx, 0) == 0.25f);

  // Click + drag inside the custom widget
  mouse_move(child, 100, 50, false);
  mouse_press(child, 100, 50);
  pump_messages(200, tick);
  mouse_move(child, 150, 50, true);
  pump_messages(200, tick);
  mouse_release(child, 150, 50);
  pump_messages(200, tick);

  const float after = fx->getParameter(fx, 0);
  CHECK(after != 0.25f);
  CHECK(after > 0.4f);

  // Automation gestures reached the host
  CHECK(g_host.begin_edits == 1);
  CHECK(g_host.end_edits == 1);
  CHECK(g_host.automates >= 1);
  CHECK(g_host.last_automated_index == 0);
  CHECK(g_host.last_automated_value == after);

  // Bus round trip: the release queued ui_to_processor{level}; a process
  // block drains it, the processor copies it into Gain (struct value) and
  // answers with feedback drained on the next editor frame.
  dispatch(EffectOpcodes::SetSampleRate, 0, 0, nullptr, 48000.f);
  dispatch(EffectOpcodes::SetBlockSize, 0, 64);
  dispatch(EffectOpcodes::MainsChanged, 0, 1);

  float in_l[64], in_r[64], out_l[64], out_r[64];
  for(int i = 0; i < 64; i++)
  {
    in_l[i] = 1.f;
    in_r[i] = 1.f;
  }
  std::memset(out_l, 0, sizeof(out_l));
  std::memset(out_r, 0, sizeof(out_r));
  float* ins[2] = {in_l, in_r};
  float* outs[2] = {out_l, out_r};
  fx->processReplacing(fx, ins, outs, 64);

  // gain (param 1) was set to the committed level by process_message, and
  // the output of this very block used it: out = in * gain * level.
  char display[256]{};
  dispatch(EffectOpcodes::GetParamDisplay, 1, 0, display);
  const float gain_now = std::strtof(display, nullptr);
  CHECK(std::abs(gain_now - after) < 0.01f); // display string is rounded
  CHECK(out_l[0] == 1.f * after * after);

  // Feedback message drains on the editor frame without incident.
  pump_messages(150, tick);

  REQUIRE(dispatch(EffectOpcodes::EditClose) == 1);
  pump_messages(50);
  destroy_parent(parent);
  dispatch(EffectOpcodes::Close);
  AVND_TEST_DLCLOSE(lib);
}

#else

TEST_CASE("vst2 editor: embed, interact, gestures, bus", "[vintage_gui]")
{
  SKIP("vst2 gui test needs Win32 or X11");
}

#endif
