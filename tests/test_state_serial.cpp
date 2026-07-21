/* SPDX-License-Identifier: GPL-3.0-or-later */

// Reflection-based state serialization: the round-trip, and the schema
// changes it is meant to absorb without migration code.

#include <avnd/wrappers/state_serial.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <map>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
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
  REQUIRE(avnd::state::load(out, bytes.data(), bytes.size()).ok);
  REQUIRE(out.a == -7);
  REQUIRE(out.b == Catch::Approx(2.5f));
  REQUIRE(out.c == true);
}

TEST_CASE("state: nested aggregates round-trip", "[state]")
{
  nested in{.id = 3, .inner = {.a = 1, .b = -0.5f, .c = true}};
  const auto bytes = avnd::state::save(in);

  nested out{};
  REQUIRE(avnd::state::load(out, bytes.data(), bytes.size()).ok);
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
  REQUIRE(avnd::state::load(out, bytes.data(), bytes.size()).ok);
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
  REQUIRE(avnd::state::load(out, bytes.data(), bytes.size()).ok);
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
  REQUIRE(avnd::state::load(out, bytes.data(), bytes.size()).ok);
  REQUIRE(out.items.size() == 3);
  REQUIRE(out.items[1].a == 2);
  REQUIRE(out.items[1].b == Catch::Approx(-2.5f));
  REQUIRE(out.items[2].a == 3);
  REQUIRE(out.trailing == 77); // the record after the list is still found
}

TEST_CASE("state: appending a member keeps its default", "[state][schema]")
{
  const v1::cfg old{.mode = 5, .gain = 0.25f};
  const auto bytes = avnd::state::save(old);

  v2_added::cfg loaded{};
  const auto res = avnd::state::load(loaded, bytes.data(), bytes.size());
  REQUIRE(res.ok);
  REQUIRE(loaded.mode == 5);
  REQUIRE(loaded.gain == Catch::Approx(0.25f));
  REQUIRE(loaded.extra == 42); // appended member keeps its default
  REQUIRE(res.layout_changed); // the change is visible, but benign
  REQUIRE_FALSE(res.type_mismatch);
}

TEST_CASE("state: dropping a trailing member skips its record",
          "[state][schema]")
{
  const v1::cfg old{.mode = 5, .gain = 0.25f};
  const auto bytes = avnd::state::save(old);

  v2_removed::cfg loaded{};
  const auto res = avnd::state::load(loaded, bytes.data(), bytes.size());
  REQUIRE(res.ok);
  REQUIRE(loaded.mode == 5); // the surviving member is still read
  REQUIRE(res.layout_changed);
  REQUIRE_FALSE(res.type_mismatch); // dropping a trailing member is benign
}

TEST_CASE("state: reordering members is reported as layout drift",
          "[state][schema]")
{
  // Positional encoding cannot follow a reorder. What it can do is notice:
  // the layout hash differs, so the caller is told rather than silently
  // handed values that landed on the wrong members.
  const v1::cfg old{.mode = 5, .gain = 0.25f};
  const auto bytes = avnd::state::save(old);

  v2_reordered::cfg loaded{};
  const auto res = avnd::state::load(loaded, bytes.data(), bytes.size());
  REQUIRE(res.type_mismatch);
  // The two members have different tags-with-sizes, so neither is applied.
  REQUIRE(loaded.mode == 0);
  REQUIRE(loaded.gain == Catch::Approx(0.f));
}

TEST_CASE("state: a member that changed type keeps its default", "[state][schema]")
{
  const v1::cfg old{.mode = 5, .gain = 0.25f};
  const auto bytes = avnd::state::save(old);

  v2_retyped::cfg loaded{};
  REQUIRE(avnd::state::load(loaded, bytes.data(), bytes.size()).ok);
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
  REQUIRE(avnd::state::load(out, nullptr, 0).ok);
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

TEST_CASE("state: an unchanged layout reports no drift", "[state][schema]")
{
  const v1::cfg a{.mode = 1, .gain = 2.f};
  const auto bytes = avnd::state::save(a);
  v1::cfg b{};
  const auto res = avnd::state::load(b, bytes.data(), bytes.size());
  REQUIRE(res.ok);
  REQUIRE_FALSE(res.layout_changed);
  REQUIRE_FALSE(res.type_mismatch);
  REQUIRE(b.mode == 1);
  REQUIRE(b.gain == Catch::Approx(2.f));
}

TEST_CASE("state: the layout hash distinguishes shapes", "[state][schema]")
{
  // Same members, different order or count: different hash.
  REQUIRE(
      avnd::state::layout_hash<v1::cfg>()
      != avnd::state::layout_hash<v2_reordered::cfg>());
  REQUIRE(
      avnd::state::layout_hash<v1::cfg>()
      != avnd::state::layout_hash<v2_added::cfg>());
  REQUIRE(
      avnd::state::layout_hash<v1::cfg>()
      != avnd::state::layout_hash<v2_retyped::cfg>());
  // Renaming a member without touching its shape is invisible, by design:
  // positionally the two structs are the same state.
  struct renamed
  {
    int operating_mode{};
    float level{};
  };
  REQUIRE(
      avnd::state::layout_hash<v1::cfg>() == avnd::state::layout_hash<renamed>());
}

TEST_CASE("state: unnamed state structs work", "[state][schema]")
{
  // The point of positional encoding: state can be declared inline, like
  // the inputs and outputs groups.
  struct Host
  {
    struct
    {
      int a{};
      std::string s;
    } state;
  };
  Host in;
  in.state.a = 9;
  in.state.s = "inline";
  const auto bytes = avnd::state::save(in.state);

  Host out;
  const auto res = avnd::state::load(out.state, bytes.data(), bytes.size());
  REQUIRE(res.ok);
  REQUIRE_FALSE(res.layout_changed);
  REQUIRE_FALSE(res.type_mismatch);
  REQUIRE(out.state.a == 9);
  REQUIRE(out.state.s == "inline");
}

TEST_CASE("state: borrowed memory is refused", "[state]")
{
  // A pointer survives a memcpy syntactically and means nothing in the
  // process that reads the session back, so these never reach a file.
  struct has_pointer
  {
    int n;
    float* buffer;
  };
  struct has_view
  {
    std::string_view sv;
  };
  static_assert(!avnd::state::serializable<float*>);
  static_assert(!avnd::state::serializable<has_pointer>);
  static_assert(!avnd::state::serializable<std::string_view>);
  static_assert(!avnd::state::serializable<has_view>);

  // Owning containers that look like views -- data() and size(), no
  // resize() -- are still fine.
  static_assert(avnd::state::serializable<std::array<int, 4>>);
  // A fixed array of non-trivial elements is not: taking it apart needs
  // reflection that does not work on tuple-like types everywhere.
  static_assert(!avnd::state::serializable<std::array<std::string, 2>>);
  SUCCEED();
}

TEST_CASE("state: the supported type surface", "[state]")
{
  // Trivially-copyable types are written as bytes, which covers optional
  // and variant of trivial alternatives.
  static_assert(avnd::state::serializable<std::optional<int>>);
  static_assert(avnd::state::serializable<std::variant<int, float>>);

  // Their non-trivial forms, and the associative/tuple containers, are not
  // supported: a processor needing one provides save/load on its state.
  static_assert(!avnd::state::serializable<std::optional<std::string>>);
  static_assert(!avnd::state::serializable<std::variant<int, std::string>>);
  static_assert(!avnd::state::serializable<std::map<std::string, int>>);
  static_assert(!avnd::state::serializable<std::pair<int, float>>);
  static_assert(!avnd::state::serializable<std::tuple<int, float>>);
  SUCCEED();
}
