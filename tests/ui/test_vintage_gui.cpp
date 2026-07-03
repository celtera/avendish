/* SPDX-License-Identifier: GPL-3.0-or-later */

// End-to-end VST2 editor test: loads the ClapUiPlug plug-in built with the
// vintage binding, opens its editor in a real parent window, drives it with
// injected mouse messages, and verifies parameter values, the automation
// gesture callbacks (audioMaster BeginEdit/Automate/EndEdit) and the
// message-bus round trip. Windows-only, matching the blit backend.

#include <catch2/catch_test_macros.hpp>

#if defined(_WIN32) && defined(AVND_TEST_VST2_PATH)

#include <avnd/binding/vintage/vintage.hpp>

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN 1
#endif
#if !defined(NOMINMAX)
#define NOMINMAX 1
#endif
#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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

static void pump_messages(int ms)
{
  const DWORD end = GetTickCount() + ms;
  for(;;)
  {
    MSG msg;
    while(PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    if(GetTickCount() >= end)
      break;
    Sleep(5);
  }
}

static HWND create_parent(int w, int h)
{
  WNDCLASSW wc{};
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"avnd_vintage_gui_test_parent";
  RegisterClassW(&wc);
  RECT r{0, 0, w, h};
  AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  return CreateWindowExW(
      0, wc.lpszClassName, L"avnd vst2 gui test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr,
      nullptr, wc.hInstance, nullptr);
}

struct child_finder
{
  HWND child{};
  static BOOL CALLBACK cb(HWND hwnd, LPARAM p)
  {
    ((child_finder*)p)->child = hwnd;
    return FALSE;
  }
};
}

TEST_CASE("vst2 editor: embed, interact, gestures, bus", "[vintage_gui]")
{
  using vintage::EffectOpcodes;

  const HMODULE lib = LoadLibraryA(AVND_TEST_VST2_PATH);
  REQUIRE(lib);
  using entry_t = vintage::Effect* (*)(vintage::HostCallback);
  const auto entry = (entry_t)GetProcAddress(lib, "VSTPluginMain");
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

  // Editor rect must be available before opening
  vintage::Rect* rect{};
  REQUIRE(dispatch(EffectOpcodes::EditGetRect, 0, 0, &rect) == 1);
  REQUIRE(rect);
  CHECK(rect->right == 320);
  CHECK(rect->bottom == 220);

  const HWND parent = create_parent(rect->right, rect->bottom);
  REQUIRE(parent);
  REQUIRE(dispatch(EffectOpcodes::EditOpen, 0, 0, parent) == 1);

  pump_messages(300);
  dispatch(EffectOpcodes::EditIdle);

  child_finder finder;
  EnumChildWindows(parent, &child_finder::cb, (LPARAM)&finder);
  REQUIRE(finder.child);

  // "Level" starts at 0.25 (param 0, range 0..1 so normalized == value)
  CHECK(fx->getParameter(fx, 0) == 0.25f);

  // Click + drag inside the custom widget
  const auto pos = [](int x, int y) { return MAKELPARAM(x, y); };
  PostMessageW(finder.child, WM_MOUSEMOVE, 0, pos(100, 50));
  PostMessageW(finder.child, WM_LBUTTONDOWN, MK_LBUTTON, pos(100, 50));
  pump_messages(200);
  PostMessageW(finder.child, WM_MOUSEMOVE, MK_LBUTTON, pos(150, 50));
  pump_messages(200);
  PostMessageW(finder.child, WM_LBUTTONUP, 0, pos(150, 50));
  pump_messages(200);
  dispatch(EffectOpcodes::EditIdle);

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
  pump_messages(150);
  dispatch(EffectOpcodes::EditIdle);

  REQUIRE(dispatch(EffectOpcodes::EditClose) == 1);
  pump_messages(50);
  DestroyWindow(parent);
  dispatch(EffectOpcodes::Close);
  FreeLibrary(lib);
}

#else

TEST_CASE("vst2 editor: embed, interact, gestures, bus", "[vintage_gui]")
{
  SKIP("vst2 gui test currently runs on Windows only");
}

#endif
