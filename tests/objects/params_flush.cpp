/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

// clap_plugin_params::flush must apply the input parameter events even when
// the plug-in is deactivated (main-thread case): values change and are
// visible through get_value afterwards.

#include <avnd/binding/clap/audio_effect.hpp>

#include <catch2/catch_all.hpp>

namespace
{
struct FlushFx
{
  static consteval auto name() { return "FlushFx"; }
  static consteval auto c_name() { return "flush_fx"; }
  static consteval auto uuid() { return "b3a5df20-6c1f-45e2-9f6e-2b8c92f0a5d3"; }

  struct
  {
    struct
    {
      static consteval auto name() { return "Gain"; }
      struct range
      {
        float min = 0.f, max = 1.f, init = 0.25f;
      };
      float value = 0.25f;
    } gain;
  } inputs;
  struct
  {
  } outputs;
  void operator()(int) { }
};

const clap_host fake_host{
    .clap_version = CLAP_VERSION,
    .host_data = nullptr,
    .name = "test-host",
    .vendor = "avnd",
    .url = "",
    .version = "1.0",
    .get_extension = [](const clap_host*, const char*) -> const void* {
      return nullptr;
    },
    .request_restart = [](const clap_host*) {},
    .request_process = [](const clap_host*) {},
    .request_callback = [](const clap_host*) {},
};

struct event_list
{
  clap_event_param_value_t ev{};
  clap_input_events_t in{
      .ctx = this,
      .size = [](const clap_input_events_t*) -> uint32_t { return 1; },
      .get = [](const clap_input_events_t* list,
                uint32_t) -> const clap_event_header_t* {
        return &static_cast<event_list*>(list->ctx)->ev.header;
      }};
  clap_output_events_t out{
      .ctx = nullptr,
      .try_push = [](const clap_output_events_t*,
                     const clap_event_header_t*) -> bool { return true; }};

  explicit event_list(clap_id id, double value)
  {
    ev.header.size = sizeof(ev);
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.param_id = id;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.value = value;
  }
};
}

TEST_CASE("clap params: flush applies events on a deactivated plug-in", "[params]")
{
  // Never activated: flush must still apply the values (main-thread case).
  auto fx = std::make_unique<avnd_clap::SimpleAudioEffect<FlushFx>>(&fake_host);
  auto* params = fx->get_params();
  REQUIRE(params != nullptr);

  event_list events{0, 0.75};
  params->flush(fx.get(), &events.in, &events.out);

  REQUIRE(fx->effect.effect.inputs.gain.value == Catch::Approx(0.75f));
  double reported = 0.;
  REQUIRE(params->get_value(fx.get(), 0, &reported));
  REQUIRE(reported == Catch::Approx(0.75));

  // Null lists are tolerated.
  params->flush(fx.get(), nullptr, nullptr);
  REQUIRE(fx->effect.effect.inputs.gain.value == Catch::Approx(0.75f));
}

namespace
{
// Non-trivial plain-value range: catches any normalized/plain confusion.
struct HzFx
{
  static consteval auto name() { return "HzFx"; }
  static consteval auto c_name() { return "hz_fx"; }
  static consteval auto uuid() { return "0d3f7a92-5b6e-4f1c-8d2a-b91e6c4a7f01"; }

  struct
  {
    struct
    {
      static consteval auto name() { return "Freq"; }
      struct range
      {
        float min = 20.f, max = 20000.f, init = 440.f;
      };
      float value = 440.f;
    } freq;
  } inputs;
  struct
  {
  } outputs;
  void operator()(int) { }
};
}

TEST_CASE("clap params: ids and values are consistent across the vtable",
          "[params]")
{
  auto fx = std::make_unique<avnd_clap::SimpleAudioEffect<HzFx>>(&fake_host);
  auto* params = fx->get_params();

  clap_param_info_t info{};
  REQUIRE(params->get_info(fx.get(), 0, &info));
  REQUIRE(info.min_value == Catch::Approx(20.));
  REQUIRE(info.max_value == Catch::Approx(20000.));

  // Write a PLAIN value through flush at the advertised id; read it back.
  event_list events{info.id, 12345.};
  params->flush(fx.get(), &events.in, &events.out);
  REQUIRE(fx->effect.effect.inputs.freq.value == Catch::Approx(12345.f));
  double reported = 0.;
  REQUIRE(params->get_value(fx.get(), info.id, &reported));
  REQUIRE(reported == Catch::Approx(12345.));
}
