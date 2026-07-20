/* SPDX-License-Identifier: GPL-3.0-or-later */

// Tier C regression test: the CustomUiWindow example ships its own raw
// editor through the avnd::gui_windowed_ui seam (no soft editor, no
// Nuklear) — raw Win32 + GDI on Windows, raw Xlib on Linux. Load its .clap,
// embed the editor, drag, and verify the gesture flow — proving a
// bring-your-own-framework UI gets the same treatment as the reference
// editor.

#include <catch2/catch_test_macros.hpp>

#if (defined(_WIN32) || defined(__linux__)) && defined(AVND_TEST_CUSTOM_UI_CLAP_PATH)

#include "gui_test_host.hpp"

#include <clap/all.h>

#if defined(_WIN32)
#define AVND_TEST_DLOPEN(path) (void*)LoadLibraryA(path)
#define AVND_TEST_DLSYM(lib, name) (void*)GetProcAddress((HMODULE)lib, name)
#define AVND_TEST_DLCLOSE(lib) FreeLibrary((HMODULE)lib)
#else
#include <dlfcn.h>
#define AVND_TEST_DLOPEN(path) dlopen(path, RTLD_NOW | RTLD_LOCAL)
#define AVND_TEST_DLSYM(lib, name) dlsym(lib, name)
#define AVND_TEST_DLCLOSE(lib) dlclose(lib)
#endif

#include <cmath>
#include <cstring>
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

// Host with clap.timer-support: Tier C editors are promised their idle()
// clock through it (that's their whole event loop on X11).
struct test_host
{
  clap_host host{};
  clap_host_timer_support timer_ext{};
  std::vector<clap_id> timers;

  test_host()
  {
    host.clap_version = CLAP_VERSION;
    host.host_data = this;
    host.name = "avnd-test-host";
    host.vendor = "avendish";
    host.url = "";
    host.version = "1.0";
    host.get_extension = [](const clap_host* h, const char* id) -> const void* {
      auto& self = *(test_host*)h->host_data;
      if(std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0)
        return &self.timer_ext;
      return nullptr;
    };
    host.request_restart = [](const clap_host*) {};
    host.request_process = [](const clap_host*) {};
    host.request_callback = [](const clap_host*) {};

    timer_ext.register_timer
        = [](const clap_host* h, uint32_t, clap_id* id) -> bool {
      auto& self = *(test_host*)h->host_data;
      *id = (clap_id)(1000 + self.timers.size());
      self.timers.push_back(*id);
      return true;
    };
    timer_ext.unregister_timer = [](const clap_host* h, clap_id id) -> bool {
      auto& self = *(test_host*)h->host_data;
      std::erase(self.timers, id);
      return true;
    };
  }
};
}

TEST_CASE("tier C: author-provided editor through the plug-in bindings", "[custom_ui]")
{
  using namespace avnd_test_gui;

  void* lib = AVND_TEST_DLOPEN(AVND_TEST_CUSTOM_UI_CLAP_PATH);
  REQUIRE(lib);
  const auto* entry = (const clap_plugin_entry*)AVND_TEST_DLSYM(lib, "clap_entry");
  REQUIRE(entry);
  REQUIRE(entry->init(AVND_TEST_CUSTOM_UI_CLAP_PATH));

  const auto* factory
      = (const clap_plugin_factory*)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
  REQUIRE(factory);
  const auto* desc = factory->get_plugin_descriptor(factory, 0);
  REQUIRE(desc);

  test_host host;
  const auto* plugin = factory->create_plugin(factory, &host.host, desc->id);
  REQUIRE(plugin);
  REQUIRE(plugin->init(plugin));

  const auto* gui = (const clap_plugin_gui*)plugin->get_extension(plugin, CLAP_EXT_GUI);
  REQUIRE(gui);
  const auto* params
      = (const clap_plugin_params*)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
  REQUIRE(params);
  const auto* timer_support = (const clap_plugin_timer_support*)plugin->get_extension(
      plugin, CLAP_EXT_TIMER_SUPPORT);
  REQUIRE(timer_support);

  const auto tick = [&] {
    for(const clap_id id : host.timers)
      timer_support->on_timer(plugin, id);
  };

#if defined(_WIN32)
  REQUIRE(gui->create(plugin, CLAP_WINDOW_API_WIN32, false));
#else
  REQUIRE(gui->create(plugin, CLAP_WINDOW_API_X11, false));
#endif
  uint32_t w{}, h{};
  REQUIRE(gui->get_size(plugin, &w, &h));
  CHECK(w == 360); // the author's window::size(), not a soft-editor layout
  CHECK(h == 120);

  const native_window parent = create_parent(w, h, L"avnd_custom_ui_test_parent");
  REQUIRE(parent);
  clap_window cw{};
#if defined(_WIN32)
  cw.api = CLAP_WINDOW_API_WIN32;
  cw.win32 = parent;
#else
  cw.api = CLAP_WINDOW_API_X11;
  cw.x11 = parent;
#endif
  REQUIRE(gui->set_parent(plugin, &cw));

  pump_messages(200, tick);

#if defined(_WIN32)
  // The author's window class, not pugl's
  const HWND child
      = FindWindowExW(parent, nullptr, L"avnd_custom_ui_window_example", nullptr);
#else
  const native_window child = find_child(parent);
#endif
  REQUIRE(child);

  double before{};
  REQUIRE(params->get_value(plugin, 0, &before));
  CHECK(before == 0.5);

  // Drag the gain bar to ~75% width
  mouse_press(child, 180, 60);
  pump_messages(100, tick);
  mouse_move(child, 270, 60, true);
  pump_messages(100, tick);
  mouse_release(child, 270, 60);
  pump_messages(100, tick);

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
  pump_messages(150, tick);

  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);

  gui->destroy(plugin);
  pump_messages(50);
  destroy_parent(parent);
  plugin->destroy(plugin);
  entry->deinit();
  AVND_TEST_DLCLOSE(lib);
}

#else

TEST_CASE("tier C: author-provided editor through the plug-in bindings", "[custom_ui]")
{
  SKIP("custom ui window test needs Win32 or X11");
}

#endif
