/* SPDX-License-Identifier: GPL-3.0-or-later */

// End-to-end CLAP editor test: loads the ClapUiPlug .clap built by the test
// suite, embeds its GUI in a real parent window, drives it with injected
// mouse messages through the message pump (pugl's timer/paint path), and
// verifies the parameter value + automation-gesture flow through the params
// extension. Windows-only for now, matching the blit backend.

#include <catch2/catch_test_macros.hpp>

#if defined(_WIN32) && defined(AVND_TEST_CLAP_PATH)

#include <clap/all.h>

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN 1
#endif
#if !defined(NOMINMAX)
#define NOMINMAX 1
#endif
#include <windows.h>

#include <cstdio>
#include <string>
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

  const clap_event_header* at(size_t i) const
  {
    return (const clap_event_header*)events[i].data();
  }

  int count(uint16_t type) const
  {
    int n = 0;
    for(size_t i = 0; i < events.size(); i++)
      n += at(i)->type == type;
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
  wc.lpszClassName = L"avnd_clap_gui_test_parent";
  RegisterClassW(&wc);
  RECT r{0, 0, w, h};
  AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  return CreateWindowExW(
      0, wc.lpszClassName, L"avnd clap gui test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
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

// Best-effort visual artifact: capture the editor window into a PPM.
static void capture_window(HWND hwnd, const char* path)
{
  RECT rc{};
  GetClientRect(hwnd, &rc);
  const int w = rc.right, h = rc.bottom;
  if(w <= 0 || h <= 0)
    return;

  const HDC win_dc = GetDC(hwnd);
  const HDC mem_dc = CreateCompatibleDC(win_dc);
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits{};
  if(const HBITMAP bmp = CreateDIBSection(win_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0))
  {
    const auto old = SelectObject(mem_dc, bmp);
    // PW_RENDERFULLCONTENT (2): ask DWM for the actual content
    if(!PrintWindow(hwnd, mem_dc, 2))
      BitBlt(mem_dc, 0, 0, w, h, win_dc, 0, 0, SRCCOPY);
    if(std::FILE* f = std::fopen(path, "wb"))
    {
      std::fprintf(f, "P6\n%d %d\n255\n", w, h);
      auto* px = (const unsigned char*)bits;
      for(int i = 0; i < w * h; i++)
      {
        const unsigned char rgb[3] = {px[i * 4 + 2], px[i * 4 + 1], px[i * 4 + 0]};
        std::fwrite(rgb, 1, 3, f);
      }
      std::fclose(f);
    }
    SelectObject(mem_dc, old);
    DeleteObject(bmp);
  }
  DeleteDC(mem_dc);
  ReleaseDC(hwnd, win_dc);
}
}

TEST_CASE("clap editor: embed, interact, gestures", "[clap_gui]")
{
  const HMODULE lib = LoadLibraryA(AVND_TEST_CLAP_PATH);
  REQUIRE(lib);
  const auto* entry = (const clap_plugin_entry*)GetProcAddress(lib, "clap_entry");
  REQUIRE(entry);
  REQUIRE(entry->init(AVND_TEST_CLAP_PATH));

  const auto* factory
      = (const clap_plugin_factory*)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
  REQUIRE(factory);
  REQUIRE(factory->get_plugin_count(factory) == 1);
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
  REQUIRE(
      (const clap_plugin_timer_support*)plugin->get_extension(
          plugin, CLAP_EXT_TIMER_SUPPORT));

  REQUIRE(gui->is_api_supported(plugin, CLAP_WINDOW_API_WIN32, false));
  REQUIRE_FALSE(gui->is_api_supported(plugin, CLAP_WINDOW_API_X11, false));
  REQUIRE(gui->create(plugin, CLAP_WINDOW_API_WIN32, false));

  uint32_t w{}, h{};
  REQUIRE(gui->get_size(plugin, &w, &h));
  REQUIRE(w == 320);
  REQUIRE(h == 220);

  const HWND parent = create_parent(w, h);
  REQUIRE(parent);

  clap_window cw{};
  cw.api = CLAP_WINDOW_API_WIN32;
  cw.win32 = parent;
  REQUIRE(gui->set_parent(plugin, &cw));
  REQUIRE(gui->show(plugin));

  // Let the pugl timer render a few frames through the message pump.
  pump_messages(300);

  child_finder finder;
  EnumChildWindows(parent, &child_finder::cb, (LPARAM)&finder);
  REQUIRE(finder.child);
  RECT rc{};
  GetClientRect(finder.child, &rc);
  CHECK(rc.right == (LONG)w);
  CHECK(rc.bottom == (LONG)h);

  // The "Level" param starts at 0.25.
  double before{};
  REQUIRE(params->get_value(plugin, 0, &before));
  CHECK(before == 0.25);

  // Click + drag inside the custom widget (container padding puts it near
  // the top-left; press at x=100 of the 200px-wide widget).
  const auto pos = [](int x, int y) { return MAKELPARAM(x, y); };
  PostMessageW(finder.child, WM_MOUSEMOVE, 0, pos(100, 50));
  PostMessageW(finder.child, WM_LBUTTONDOWN, MK_LBUTTON, pos(100, 50));
  pump_messages(200);
  PostMessageW(finder.child, WM_MOUSEMOVE, MK_LBUTTON, pos(150, 50));
  pump_messages(200);
  PostMessageW(finder.child, WM_LBUTTONUP, 0, pos(150, 50));
  pump_messages(200);

  capture_window(finder.child, "clap_gui_editor.ppm");

  double after{};
  REQUIRE(params->get_value(plugin, 0, &after));
  CHECK(after != before);
  CHECK(after > 0.4); // pressed at ~half the widget width, dragged right

  // The editor queued automation gestures; a host param flush must emit
  // GESTURE_BEGIN / PARAM_VALUE / GESTURE_END events.
  out_event_collector out;
  params->flush(plugin, &empty_in_events, &out.list);
  CHECK(out.count(CLAP_EVENT_PARAM_GESTURE_BEGIN) == 1);
  CHECK(out.count(CLAP_EVENT_PARAM_GESTURE_END) == 1);
  CHECK(out.count(CLAP_EVENT_PARAM_VALUE) >= 1);

  // Message bus round trip: releasing the mouse queued a ui->processor
  // message (lock-free queue); process() drains it, the processor copies
  // the value into the Gain param and answers with a feedback message
  // drained on the next editor timer tick.
  double gain_before{};
  REQUIRE(params->get_value(plugin, 1, &gain_before));
  CHECK(gain_before == 0.5);

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

  double gain_after{};
  REQUIRE(params->get_value(plugin, 1, &gain_after));
  CHECK(gain_after == after); // processor received the committed level

  // Feedback message drains on the editor timer without incident.
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

TEST_CASE("clap editor: embed, interact, gestures", "[clap_gui]")
{
  SKIP("clap gui test currently runs on Windows only");
}

#endif
