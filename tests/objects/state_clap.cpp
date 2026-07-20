/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

// Runtime round-trip of the custom-state feature through the real CLAP
// binding: params + opaque blob, saved into an in-memory clap_ostream and
// loaded back through a clap_istream.

#include <avnd/binding/clap/audio_effect.hpp>

#include <catch2/catch_all.hpp>

#include <cstring>
#include <vector>

namespace
{
// A processor with one parameter and a little versioned custom-state blob.
struct StatefulFx
{
  static consteval auto name() { return "StatefulFx"; }
  static consteval auto c_name() { return "stateful_fx"; }
  static consteval auto uuid() { return "5a0175c4-46ff-4b3c-8f5f-6b60acbdc9f0"; }

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

  // Custom state: a little versioned blob owning `secret`.
  int secret = 42;

  std::vector<uint8_t> save_state()
  {
    std::vector<uint8_t> v(8);
    const uint32_t magic = 0x5EC2E7;
    std::memcpy(v.data(), &magic, 4);
    std::memcpy(v.data() + 4, &secret, 4);
    return v;
  }

  bool load_state(const uint8_t* data, size_t n)
  {
    if(n != 8)
      return false;
    uint32_t magic{};
    std::memcpy(&magic, data, 4);
    if(magic != 0x5EC2E7)
      return false;
    std::memcpy(&secret, data + 4, 4);
    return true;
  }

  void operator()(int frames) { }
};
static_assert(avnd::has_custom_state<StatefulFx>);

// A processor without custom state: only tier-0 params.
struct PlainFx
{
  static consteval auto name() { return "PlainFx"; }
  static consteval auto c_name() { return "plain_fx"; }
  static consteval auto uuid() { return "e6b6f4de-9d3e-4e35-9d5f-3f2b7bd9b2af"; }

  struct
  {
    struct
    {
      static consteval auto name() { return "Amount"; }
      struct range
      {
        float min = 0.f, max = 1.f, init = 0.5f;
      };
      float value = 0.5f;
    } amount;
  } inputs;

  struct
  {
  } outputs;

  void operator()(int frames) { }
};
static_assert(!avnd::has_custom_state<PlainFx>);

// In-memory clap streams.
struct memory_streams
{
  std::vector<uint8_t> bytes;
  size_t read_pos = 0;
  // Force tiny transfers to exercise the partial-IO loops.
  int64_t max_chunk = 3;

  clap_ostream out{
      .ctx = this,
      .write = [](const clap_ostream* stream, const void* buffer,
                  uint64_t size) -> int64_t {
        auto& self = *static_cast<memory_streams*>(stream->ctx);
        const auto n = std::min<uint64_t>(size, self.max_chunk);
        auto b = static_cast<const uint8_t*>(buffer);
        self.bytes.insert(self.bytes.end(), b, b + n);
        return int64_t(n);
      }};

  clap_istream in{
      .ctx = this,
      .read = [](const clap_istream* stream, void* buffer,
                 uint64_t size) -> int64_t {
        auto& self = *static_cast<memory_streams*>(stream->ctx);
        const auto avail = self.bytes.size() - self.read_pos;
        const auto n = std::min<uint64_t>(
            {size, avail, uint64_t(self.max_chunk)});
        std::memcpy(buffer, self.bytes.data() + self.read_pos, n);
        self.read_pos += n;
        return int64_t(n);
      }};
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

TEST_CASE("clap state: extension is advertised", "[state]")
{
  auto fx = std::make_unique<avnd_clap::SimpleAudioEffect<StatefulFx>>(&fake_host);
  auto ext = fx->get_extension(fx.get(), CLAP_EXT_STATE);
  REQUIRE(ext != nullptr);
  REQUIRE(ext == &fx->state_ext);
}

TEST_CASE("clap state: params + custom blob round-trip", "[state]")
{
  auto fx = std::make_unique<avnd_clap::SimpleAudioEffect<StatefulFx>>(&fake_host);

  // Mutate both tiers.
  fx->effect.effect.inputs.gain.value = 0.75f;
  fx->effect.effect.secret = 1234;

  memory_streams mem;
  REQUIRE(fx->state_ext.save(fx.get(), &mem.out));
  REQUIRE(mem.bytes.size() > 0);

  // Wreck the live state, then load the snapshot back.
  fx->effect.effect.inputs.gain.value = 0.f;
  fx->effect.effect.secret = -1;
  REQUIRE(fx->state_ext.load(fx.get(), &mem.in));

  REQUIRE(fx->effect.effect.inputs.gain.value == Catch::Approx(0.75f));
  REQUIRE(fx->effect.effect.secret == 1234);
}

TEST_CASE("clap state: rejected blob leaves object playable", "[state]")
{
  auto fx = std::make_unique<avnd_clap::SimpleAudioEffect<StatefulFx>>(&fake_host);
  fx->effect.effect.secret = 77;

  memory_streams mem;
  REQUIRE(fx->state_ext.save(fx.get(), &mem.out));

  // Corrupt the custom blob's magic (it sits at the end of the container).
  mem.bytes[mem.bytes.size() - 8] ^= 0xFF;
  REQUIRE(!fx->state_ext.load(fx.get(), &mem.in));

  // Truncated container.
  memory_streams trunc;
  trunc.bytes.assign(mem.bytes.begin(), mem.bytes.begin() + 5);
  REQUIRE(!fx->state_ext.load(fx.get(), &trunc.in));

  // Bad magic.
  memory_streams bad;
  bad.bytes = mem.bytes;
  bad.bytes[0] = 0x00;
  REQUIRE(!fx->state_ext.load(fx.get(), &bad.in));
}

TEST_CASE("clap state: tier-0 only (no custom state)", "[state]")
{
  auto fx = std::make_unique<avnd_clap::SimpleAudioEffect<PlainFx>>(&fake_host);
  fx->effect.effect.inputs.amount.value = 0.9f;

  memory_streams mem;
  REQUIRE(fx->state_ext.save(fx.get(), &mem.out));

  fx->effect.effect.inputs.amount.value = 0.1f;
  REQUIRE(fx->state_ext.load(fx.get(), &mem.in));
  REQUIRE(fx->effect.effect.inputs.amount.value == Catch::Approx(0.9f));
}
