/* SPDX-License-Identifier: GPL-3.0-or-later */

// The processor-facing state API: automatic reflection by default, explicit
// save/load when the object provides it, and version migration.

#include <avnd/wrappers/state_object.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace test_state_objects
{
// Reflected: no save/load anywhere.
struct Plain
{
  struct
  {
    int mode{7};
    float gain{0.5f};
    std::string label{"init"};
  } state;
};

// The state object serializes itself; reflection must not be used.
struct SelfSerializing
{
  struct
  {
    int value{0};
    int load_calls{0};

    std::vector<std::uint8_t> save() const
    {
      return {std::uint8_t(value & 0xFF), 0xAB};
    }
    bool load(const std::uint8_t* d, std::size_t n)
    {
      if(n != 2 || d[1] != 0xAB)
        return false; // reject anything not written by us
      value = d[0];
      load_calls++;
      return true;
    }
  } state;
};

// Versioned with a migration: v1 stored gain normalized, v2 stores Hz.
struct Versioned
{
  static constexpr int state_version = 2;

  struct
  {
    float cutoff{1000.f};
    int migrated_from{-1};
  } state;

  static void migrate_state(decltype(state)& s, int from)
  {
    s.migrated_from = from;
    if(from < 2)
      s.cutoff = s.cutoff * 20000.f; // normalized -> Hz
  }
};

// No state member at all.
struct Stateless
{
  struct
  {
    int x{};
  } inputs;
};
}

using namespace test_state_objects;

TEST_CASE("state object: detection", "[state]")
{
  static_assert(avnd::has_state<Plain>);
  static_assert(avnd::persistable<Plain>);
  static_assert(avnd::state_is_reflectable<Plain>);
  static_assert(!avnd::state_saves_itself<Plain>);

  static_assert(avnd::state_saves_itself<SelfSerializing>);
  static_assert(avnd::state_loads_itself<SelfSerializing>);
  static_assert(avnd::persistable<SelfSerializing>);

  static_assert(!avnd::has_state<Stateless>);
  static_assert(!avnd::persistable<Stateless>);

  static_assert(avnd::state_version<Plain>() == 0);
  static_assert(avnd::state_version<Versioned>() == 2);
  static_assert(avnd::has_migration<Versioned>);
  static_assert(!avnd::has_migration<Plain>);
  SUCCEED();
}

TEST_CASE("state object: reflected round-trip", "[state]")
{
  Plain a;
  a.state.mode = 42;
  a.state.gain = 0.25f;
  a.state.label = "saved";
  const auto blob = avnd::save_object_state(a);

  Plain b;
  REQUIRE(avnd::load_object_state(b, blob.data(), blob.size(), 0));
  REQUIRE(b.state.mode == 42);
  REQUIRE(b.state.gain == Catch::Approx(0.25f));
  REQUIRE(b.state.label == "saved");
}

TEST_CASE("state object: an explicit save/load overrides reflection", "[state]")
{
  SelfSerializing a;
  a.state.value = 99;
  const auto blob = avnd::save_object_state(a);
  REQUIRE(blob.size() == 2); // its own format, not the reflected one
  REQUIRE(blob[1] == 0xAB);

  SelfSerializing b;
  REQUIRE(avnd::load_object_state(b, blob.data(), blob.size(), 0));
  REQUIRE(b.state.value == 99);
  REQUIRE(b.state.load_calls == 1);

  // Its own rejection is honoured.
  const std::uint8_t junk[3]{1, 2, 3};
  SelfSerializing c;
  REQUIRE(!avnd::load_object_state(c, junk, 3, 0));
  REQUIRE(c.state.value == 0);
  REQUIRE(c.state.load_calls == 0);
}

TEST_CASE("state object: migration runs for older data only", "[state][version]")
{
  Versioned a;
  a.state.cutoff = 0.5f; // as a v1 build would have stored it
  const auto blob = avnd::save_object_state(a);

  // Loading v1 data into the v2 binary migrates it.
  Versioned from_v1;
  REQUIRE(avnd::load_object_state(from_v1, blob.data(), blob.size(), 1));
  REQUIRE(from_v1.state.migrated_from == 1);
  REQUIRE(from_v1.state.cutoff == Catch::Approx(10000.f));

  // Loading current-version data does not migrate.
  Versioned current;
  REQUIRE(avnd::load_object_state(current, blob.data(), blob.size(), 2));
  REQUIRE(current.state.migrated_from == -1);
  REQUIRE(current.state.cutoff == Catch::Approx(0.5f));
}

TEST_CASE("state object: data from a newer version is refused", "[state][version]")
{
  Versioned a;
  a.state.cutoff = 123.f;
  const auto blob = avnd::save_object_state(a);

  Versioned b;
  const float before = b.state.cutoff;
  // Written by a hypothetical v3 build: refuse rather than misread.
  REQUIRE(!avnd::load_object_state(b, blob.data(), blob.size(), 3));
  REQUIRE(b.state.cutoff == Catch::Approx(before));
  REQUIRE(b.state.migrated_from == -1);
}

TEST_CASE("state object: unversioned processors round-trip at version 0", "[state]")
{
  Plain a;
  a.state.mode = 3;
  const auto blob = avnd::save_object_state(a);
  Plain b;
  REQUIRE(avnd::load_object_state(b, blob.data(), blob.size(), 0));
  REQUIRE(b.state.mode == 3);
}
