/* SPDX-License-Identifier: GPL-3.0-or-later */

// Reflection-based state serialization: the round-trip, and the schema
// changes it is meant to absorb without migration code.

#include <avnd/wrappers/state_serial.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace test_state_types
{
struct simple
{
  int a{};
  float b{};
  bool c{};
};

struct nested
{
  int id{};
  simple inner{};
};

struct with_containers
{
  std::string name;
  std::vector<std::uint8_t> blob;
  std::vector<int> numbers;
};

struct with_list
{
  std::vector<simple> items;
  int trailing{};
};

// Schema evolution: same struct name, different members.
namespace v1
{
struct cfg
{
  int mode{};
  float gain{};
};
}
namespace v2_added
{
struct cfg
{
  int mode{};
  float gain{};
  int extra{42}; // added since v1
};
}
namespace v2_removed
{
struct cfg
{
  int mode{}; // gain removed
};
}
namespace v2_reordered
{
struct cfg
{
  float gain{};
  int mode{}; // swapped order
};
}
namespace v2_retyped
{
struct cfg
{
  int mode{};
  std::string gain{"untouched"}; // gain changed type
};
}
}

using namespace test_state_types;
TEST_CASE("state: aggregates round-trip", "[state]")
{
  simple in{.a = -7, .b = 2.5f, .c = true};
  const auto bytes = avnd::state::save(in);
  REQUIRE(!bytes.empty());

  simple out{};
  REQUIRE(avnd::state::load(out, bytes.data(), bytes.size()));
  REQUIRE(out.a == -7);
  REQUIRE(out.b == Catch::Approx(2.5f));
  REQUIRE(out.c == true);
}

TEST_CASE("state: nested aggregates round-trip", "[state]")
{
  nested in{.id = 3, .inner = {.a = 1, .b = -0.5f, .c = true}};
  const auto bytes = avnd::state::save(in);

  nested out{};
  REQUIRE(avnd::state::load(out, bytes.data(), bytes.size()));
  REQUIRE(out.id == 3);
  REQUIRE(out.inner.a == 1);
  REQUIRE(out.inner.b == Catch::Approx(-0.5f));
  REQUIRE(out.inner.c == true);
}

TEST_CASE("state: strings and vectors round-trip", "[state]")
{
  with_containers in;
  in.name = "hello state";
  in.blob = {0, 1, 2, 250, 255};
  in.numbers = {-1, 0, 99999};
  const auto bytes = avnd::state::save(in);

  with_containers out;
  REQUIRE(avnd::state::load(out, bytes.data(), bytes.size()));
  REQUIRE(out.name == "hello state");
  REQUIRE(out.blob == std::vector<std::uint8_t>{0, 1, 2, 250, 255});
  REQUIRE(out.numbers == std::vector<int>{-1, 0, 99999});
}

TEST_CASE("state: empty containers round-trip", "[state]")
{
  with_containers in; // all empty
  const auto bytes = avnd::state::save(in);

  with_containers out;
  out.name = "clobber me";
  out.numbers = {1, 2, 3};
  REQUIRE(avnd::state::load(out, bytes.data(), bytes.size()));
  REQUIRE(out.name.empty());
  REQUIRE(out.numbers.empty());
}

TEST_CASE("state: lists of aggregates round-trip", "[state]")
{
  with_list in;
  in.items = {{1, 1.5f, false}, {2, -2.5f, true}, {3, 0.f, false}};
  in.trailing = 77;
  const auto bytes = avnd::state::save(in);

  with_list out;
  REQUIRE(avnd::state::load(out, bytes.data(), bytes.size()));
  REQUIRE(out.items.size() == 3);
  REQUIRE(out.items[1].a == 2);
  REQUIRE(out.items[1].b == Catch::Approx(-2.5f));
  REQUIRE(out.items[2].a == 3);
  REQUIRE(out.trailing == 77); // the record after the list is still found
}

TEST_CASE("state: a member added later keeps its default", "[state][schema]")
{
  const v1::cfg old{.mode = 5, .gain = 0.25f};
  const auto bytes = avnd::state::save(old);

  v2_added::cfg loaded{};
  REQUIRE(avnd::state::load(loaded, bytes.data(), bytes.size()));
  REQUIRE(loaded.mode == 5);
  REQUIRE(loaded.gain == Catch::Approx(0.25f));
  REQUIRE(loaded.extra == 42); // untouched default, no migration code
}

TEST_CASE("state: a removed member is skipped", "[state][schema]")
{
  const v1::cfg old{.mode = 5, .gain = 0.25f};
  const auto bytes = avnd::state::save(old);

  v2_removed::cfg loaded{};
  REQUIRE(avnd::state::load(loaded, bytes.data(), bytes.size()));
  REQUIRE(loaded.mode == 5); // the record after the unknown one still lands
}

TEST_CASE("state: reordering members does not shuffle values", "[state][schema]")
{
  const v1::cfg old{.mode = 5, .gain = 0.25f};
  const auto bytes = avnd::state::save(old);

  v2_reordered::cfg loaded{};
  REQUIRE(avnd::state::load(loaded, bytes.data(), bytes.size()));
  REQUIRE(loaded.mode == 5);
  REQUIRE(loaded.gain == Catch::Approx(0.25f));
}

TEST_CASE("state: a member that changed type keeps its default", "[state][schema]")
{
  const v1::cfg old{.mode = 5, .gain = 0.25f};
  const auto bytes = avnd::state::save(old);

  v2_retyped::cfg loaded{};
  REQUIRE(avnd::state::load(loaded, bytes.data(), bytes.size()));
  REQUIRE(loaded.mode == 5);
  // The float bytes are NOT reinterpreted as a string.
  REQUIRE(loaded.gain == "untouched");
}

TEST_CASE("state: truncated and corrupt blobs are rejected", "[state]")
{
  with_containers in;
  in.name = "some name";
  in.blob = {1, 2, 3, 4};
  in.numbers = {5, 6};
  const auto bytes = avnd::state::save(in);

  // Every truncation is either rejected or leaves a readable prefix; none
  // may read out of bounds (ASan/valgrind territory) or hang.
  for(std::size_t n = 0; n < bytes.size(); n++)
  {
    with_containers out;
    (void)avnd::state::load(out, bytes.data(), n);
  }

  // A record claiming a huge payload must not be believed.
  auto corrupt = bytes;
  for(std::size_t i = 0; i + 4 < corrupt.size(); i++)
  {
    auto c = corrupt;
    std::uint32_t huge = 0xFFFFFFF0u;
    std::memcpy(c.data() + i, &huge, sizeof(huge));
    with_containers out;
    (void)avnd::state::load(out, c.data(), c.size());
  }
  SUCCEED("no out-of-bounds access or hang on truncated/corrupt input");
}

TEST_CASE("state: empty input leaves the target untouched", "[state]")
{
  simple out{.a = 11, .b = 1.f, .c = true};
  REQUIRE(avnd::state::load(out, nullptr, 0));
  REQUIRE(out.a == 11); // nothing said about it, nothing changed
}

TEST_CASE("state: serializability is detected statically", "[state]")
{
  struct deep
  {
    int ok{};
    std::vector<std::vector<std::string>> nested_lists;
  };
  static_assert(avnd::state::serializable<simple>);
  static_assert(avnd::state::serializable<nested>);
  static_assert(avnd::state::serializable<with_containers>);
  static_assert(avnd::state::serializable<with_list>);
  // Nested lists resolve too: list -> list -> bytes.
  static_assert(avnd::state::serializable<deep>);
  SUCCEED();
}
