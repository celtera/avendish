/* SPDX-License-Identifier: GPL-3.0-or-later */

// Tier C regression test: the CustomUiWindow example ships its own raw
// Win32 editor through the avnd::gui_windowed_ui seam (no soft editor, no
// Nuklear). Load its .clap, embed the editor, drag, and verify the gesture
// flow — proving a bring-your-own-framework UI gets the same treatment as
// the reference editor.

#include <catch2/catch_test_macros.hpp>

#if defined(_WIN32) && defined(AVND_TEST_CUSTOM_UI_CLAP_PATH)

#include <clap/all.h>

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN 1
#endif
#if !defined(NOMINMAX)
#define NOMINMAX 1
#endif
#include <windows.h>

#include <cmath>
#include <vector>

namespace
{
struct out_event_collector
{
  clap_output_events list{};
  std::vector<std::vector<char>> events;

  out_event_collector()
  {
    list.ctx = this;
    list.try_push
        = [](const clap_output_events* list, const clap_event_header* event) -> bool {
      auto& self = *(out_event_collector*)list->ctx;
      auto* bytes = (const char*)event;
      self.events.emplace_back(bytes, bytes + event->size);
      return true;
    };
  }

  int count(uint16_t type) const
  {
    int n = 0;
    for(auto& e : events)
      n += ((const clap_event_header*)e.data())->type == type;
    return n;
  }
};

static const clap_input_events empty_in_events = {
    .ctx = nullptr,
    .size = [](const clap_input_events*) -> uint32_t { return 0; },
    .get = [](const clap_input_events*,
              uint32_t) -> const clap_event_header* { return nullptr; }};

static clap_host make_host()
{
  clap_host h{};
  h.clap_version = CLAP_VERSION;
  h.name = "avnd-test-host";
  h.vendor = "avendish";
  h.url = "";
  h.version = "1.0";
  h.get_extension
      = [](const clap_host*, const char*) -> const void* { return nullptr; };
  h.request_restart = [](const clap_host*) {};
  h.request_process = [](const clap_host*) {};
  h.request_callback = [](const clap_host*) {};
  return h;
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
  wc.lpszClassName = L"avnd_custom_ui_test_parent";
  RegisterClassW(&wc);
  RECT r{0, 0, w, h};
  AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  return CreateWindowExW(
      0, wc.lpszClassName, L"avnd custom ui test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr,
      nullptr, wc.hInstance, nullptr);
}
}

TEST_CASE("tier C: author-provided editor through the plug-in bindings", "[custom_ui]")
{
  const HMODULE lib = LoadLibraryA(AVND_TEST_CUSTOM_UI_CLAP_PATH);
  REQUIRE(lib);
  const auto* entry = (const clap_plugin_entry*)GetProcAddress(lib, "clap_entry");
  REQUIRE(entry);
  REQUIRE(entry->init(AVND_TEST_CUSTOM_UI_CLAP_PATH));

  const auto* factory
      = (const clap_plugin_factory*)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
  REQUIRE(factory);
  const auto* desc = factory->get_plugin_descriptor(factory, 0);
  REQUIRE(desc);

  clap_host host = make_host();
  const auto* plugin = factory->create_plugin(factory, &host, desc->id);
  REQUIRE(plugin);
  REQUIRE(plugin->init(plugin));

  const auto* gui = (const clap_plugin_gui*)plugin->get_extension(plugin, CLAP_EXT_GUI);
  REQUIRE(gui);
  const auto* params
      = (const clap_plugin_params*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
  REQUIRE(params);

  REQUIRE(gui->create(plugin, CLAP_WINDOW_API_WIN32, false));
  uint32_t w{}, h{};
  REQUIRE(gui->get_size(plugin, &w, &h));
  CHECK(w == 360); // the author's window::size(), not a soft-editor layout
  CHECK(h == 120);

  const HWND parent = create_parent(w, h);
  REQUIRE(parent);
  clap_window cw{};
  cw.api = CLAP_WINDOW_API_WIN32;
  cw.win32 = parent;
  REQUIRE(gui->set_parent(plugin, &cw));

  pump_messages(200);

  // The author's window class, not pugl's
  const HWND child
      = FindWindowExW(parent, nullptr, L"avnd_custom_ui_window_example", nullptr);
  REQUIRE(child);

  double before{};
  REQUIRE(params->get_value(plugin, 0, &before));
  CHECK(before == 0.5);

  // Drag the gain bar to ~75% width
  const auto pos = [](int x, int y) { return MAKELPARAM(x, y); };
  PostMessageW(child, WM_LBUTTONDOWN, MK_LBUTTON, pos(180, 60));
  pump_messages(100);
  PostMessageW(child, WM_MOUSEMOVE, MK_LBUTTON, pos(270, 60));
  pump_messages(100);
  PostMessageW(child, WM_LBUTTONUP, 0, pos(270, 60));
  pump_messages(100);

  double after{};
  REQUIRE(params->get_value(plugin, 0, &after));
  CHECK(after > 0.7);

  // The same gesture queue as the soft editor serves author editors
  out_event_collector out;
  params->flush(plugin, &empty_in_events, &out.list);
  CHECK(out.count(CLAP_EVENT_PARAM_GESTURE_BEGIN) == 1);
  CHECK(out.count(CLAP_EVENT_PARAM_GESTURE_END) == 1);
  CHECK(out.count(CLAP_EVENT_PARAM_VALUE) >= 1);

  // Tier C message bus: the release sent ui_to_processor{gain} through the
  // window's send_message; process() drains it and the processor sets
  // gain = value/2 (deliberately different from the direct edit).
  REQUIRE(plugin->activate(plugin, 48000., 64, 64));
  REQUIRE(plugin->start_processing(plugin));

  float in_l[64]{}, in_r[64]{}, out_l[64]{}, out_r[64]{};
  float* ins[2] = {in_l, in_r};
  float* outs[2] = {out_l, out_r};
  clap_audio_buffer in_bus{};
  in_bus.data32 = ins;
  in_bus.channel_count = 2;
  clap_audio_buffer out_bus{};
  out_bus.data32 = outs;
  out_bus.channel_count = 2;

  out_event_collector process_out;
  clap_process proc{};
  proc.frames_count = 64;
  proc.audio_inputs = &in_bus;
  proc.audio_inputs_count = 1;
  proc.audio_outputs = &out_bus;
  proc.audio_outputs_count = 1;
  proc.in_events = &empty_in_events;
  proc.out_events = &process_out.list;
  REQUIRE(plugin->process(plugin, &proc) != CLAP_PROCESS_ERROR);

  double gain_after_bus{};
  REQUIRE(params->get_value(plugin, 0, &gain_after_bus));
  CHECK(std::abs(gain_after_bus - after * 0.5) < 1e-6);

  // The feedback message drains into window.process_message on the timer.
  pump_messages(150);

  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);

  gui->destroy(plugin);
  pump_messages(50);
  DestroyWindow(parent);
  plugin->destroy(plugin);
  entry->deinit();
  FreeLibrary(lib);
}

#else

TEST_CASE("tier C: author-provided editor through the plug-in bindings", "[custom_ui]")
{
  SKIP("custom ui window test currently runs on Windows only");
}

#endif
