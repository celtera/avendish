/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

// Host parameter changes must invoke the ports' update() callbacks with the
// new value already written, exactly once per change:
//  - through the CLAP binding's parameter-event application;
//  - through the VST3 parameter application path (stv3::apply_control);
//  - after a bulk value restore (state load).

#include <avnd/binding/clap/audio_effect.hpp>
#include <avnd/binding/vst3/param_apply.hpp>

#include <catch2/catch_all.hpp>

namespace
{
struct UpdateFx
{
  static consteval auto name() { return "UpdateFx"; }
  static consteval auto c_name() { return "update_fx"; }
  static consteval auto uuid() { return "9a2c6a2e-08a1-4a7b-9930-45a86e4dbb1c"; }

  struct
  {
    struct
    {
      static consteval auto name() { return "Freq"; }
      struct range
      {
        float min = 0.f, max = 100.f, init = 10.f;
      };
      float value = 10.f;

      void update(UpdateFx& self)
      {
        self.update_count++;
        self.observed = value;
      }
    } freq;

    struct
    {
      static consteval auto name() { return "Plain"; }
      struct range
      {
        float min = 0.f, max = 1.f, init = 0.f;
      };
      float value = 0.f;
      // no update(): must be a no-op in every path
    } plain;
  } inputs;

  struct
  {
  } outputs;

  int update_count = 0;
  float observed = -1.f;

  void operator()(int frames) { }
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
}

TEST_CASE("clap: param event fires update() once with the new value", "[update]")
{
  auto fx = std::make_unique<avnd_clap::SimpleAudioEffect<UpdateFx>>(&fake_host);
  auto& obj = fx->effect.effect;
  // Construction runs init_controls, which fires update() once per port.
  const int base = obj.update_count;

  clap_event_param_value ev{};
  ev.param_id = 0; // Freq
  ev.value = 50.;  // plain value, per the advertised min/max
  fx->process_param(ev);

  REQUIRE(obj.update_count == base + 1);
  REQUIRE(obj.observed == Catch::Approx(50.f));
  REQUIRE(obj.inputs.freq.value == Catch::Approx(50.f));

  // A port without update() is applied silently.
  clap_event_param_value ev2{};
  ev2.param_id = 1; // Plain
  ev2.value = 1.;
  fx->process_param(ev2);
  REQUIRE(obj.update_count == base + 1);
  REQUIRE(obj.inputs.plain.value == Catch::Approx(1.f));

  // One invocation per change.
  fx->process_param(ev);
  REQUIRE(obj.update_count == base + 2);
}

TEST_CASE("vst3: apply_control fires update() once with the new value", "[update]")
{
  avnd::effect_container<UpdateFx> fx;
  auto& obj = fx.effect;
  const int base = obj.update_count;

  // Raw field index 0 == Freq.
  stv3::apply_control(fx, 0, 0.25);
  REQUIRE(obj.update_count == base + 1);
  REQUIRE(obj.observed == Catch::Approx(25.f));
  REQUIRE(obj.inputs.freq.value == Catch::Approx(25.f));

  stv3::apply_control(fx, 1, 1.0); // Plain: no callback
  REQUIRE(obj.update_count == base + 1);
  REQUIRE(obj.inputs.plain.value == Catch::Approx(1.f));

  stv3::apply_control(fx, 0, 0.75);
  REQUIRE(obj.update_count == base + 2);
  REQUIRE(obj.observed == Catch::Approx(75.f));
}

TEST_CASE("clap: state load dispatches update() with restored values", "[update]")
{
  auto fx = std::make_unique<avnd_clap::SimpleAudioEffect<UpdateFx>>(&fake_host);
  auto& obj = fx->effect.effect;

  struct memory_streams
  {
    std::vector<uint8_t> bytes;
    size_t read_pos = 0;
    clap_ostream out{
        .ctx = this,
        .write = [](const clap_ostream* stream, const void* buffer,
                    uint64_t size) -> int64_t {
          auto& self = *static_cast<memory_streams*>(stream->ctx);
          auto b = static_cast<const uint8_t*>(buffer);
          self.bytes.insert(self.bytes.end(), b, b + size);
          return int64_t(size);
        }};
    clap_istream in{
        .ctx = this,
        .read = [](const clap_istream* stream, void* buffer,
                   uint64_t size) -> int64_t {
          auto& self = *static_cast<memory_streams*>(stream->ctx);
          const auto n = std::min<uint64_t>(size, self.bytes.size() - self.read_pos);
          std::memcpy(buffer, self.bytes.data() + self.read_pos, n);
          self.read_pos += n;
          return int64_t(n);
        }};
  } mem;

  obj.inputs.freq.value = 80.f;
  REQUIRE(fx->state_ext.save(fx.get(), &mem.out));

  obj.inputs.freq.value = 10.f;
  const int base = obj.update_count;
  REQUIRE(fx->state_ext.load(fx.get(), &mem.in));

  REQUIRE(obj.inputs.freq.value == Catch::Approx(80.f));
  REQUIRE(obj.observed == Catch::Approx(80.f)); // update() saw the restore
  REQUIRE(obj.update_count > base);
}

TEST_CASE("clap: params flush dispatches update() on a deactivated plug-in",
          "[update]")
{
  auto fx = std::make_unique<avnd_clap::SimpleAudioEffect<UpdateFx>>(&fake_host);
  auto& obj = fx->effect.effect;
  const int base = obj.update_count;

  clap_event_param_value_t ev{};
  ev.header.size = sizeof(ev);
  ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  ev.header.type = CLAP_EVENT_PARAM_VALUE;
  ev.param_id = 0; // Freq
  ev.note_id = -1;
  ev.port_index = -1;
  ev.channel = -1;
  ev.key = -1;
  ev.value = 50.; // plain value

  struct one_event
  {
    const clap_event_header_t* h;
    clap_input_events_t in{
        .ctx = this,
        .size = [](const clap_input_events_t*) -> uint32_t { return 1; },
        .get = [](const clap_input_events_t* list,
                  uint32_t) -> const clap_event_header_t* {
          return static_cast<one_event*>(list->ctx)->h;
        }};
  } events{&ev.header};

  fx->get_params()->flush(fx.get(), &events.in, nullptr);

  REQUIRE(obj.inputs.freq.value == Catch::Approx(50.f));
  REQUIRE(obj.update_count == base + 1); // exactly one dispatch
  REQUIRE(obj.observed == Catch::Approx(50.f));
}
