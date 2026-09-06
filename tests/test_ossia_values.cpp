#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/variant2/variant.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bitset>
#include <cstdlib>
#include <list>
#include <map>
#include <memory_resource>
#include <new>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

TEST_CASE("ossia scalar and container roundtrips")
{
  using namespace avnd;
  using namespace oscr;
  auto test = []<typename T>(T v) {
    ossia::value val = to_ossia_value(v);
    T t;
    REQUIRE(from_ossia_value(val, t));
    REQUIRE(t == v);
    return t;
  };

  static_assert(avnd::set_ish<std::set<int>>);
  static_assert(avnd::set_ish<std::unordered_set<int>>);
  static_assert(avnd::set_ish<boost::container::flat_set<int>>);
  static_assert(avnd::map_ish<std::map<int, int>>);
  static_assert(avnd::map_ish<std::unordered_map<int, int>>);
  static_assert(avnd::map_ish<boost::container::flat_map<int, int>>);

  test.operator()<int>(1);
  test.operator()<float>(1.0);
  test.operator()<std::vector<int>>(std::vector<int>{1, 2, 3});
  test.operator()<std::array<int, 2>>(std::array<int, 2>{1, 2});
  test.operator()<std::array<float, 2>>(std::array<float, 2>{1., 2.});
  test.operator()<std::bitset<64>>(std::bitset<64>{123ULL});
  test.operator()<std::set<int>>(std::set<int>{1, 3, 5, 7});
  test.operator()<std::map<int, int>>(std::map<int, int>{{1, 3}, {2, 5}});

  struct Agg
  {
    bool operator==(const Agg&) const noexcept = default;
    int a;
    float b;
    bool c;
    char d;
    std::string e;
    std::vector<int> f;
    std::array<int, 2> g;
    std::bitset<64> h;
    std::set<int> i;
    std::map<int, int> j;
    std::set<std::string> k;
    std::map<int, std::string> l;
  };

  test.operator()<Agg>(Agg{});
  test.operator()<Agg>(
      Agg{1,
          0.5f,
          true,
          'x',
          "hello",
          {1, 3, 5},
          {6, 7},
          std::bitset<64>{1234ULL},
          {1, -1},
          {{1, 2}, {3, 4}},
          {"foo", "bar"},
          {{1, "foo"}, {3, "bar"}}});
}

namespace
{
thread_local bool count_new = false;
thread_local std::size_t new_calls = 0;

template <typename F>
std::size_t allocations_during(F&& f)
{
  struct guard
  {
    guard()
    {
      new_calls = 0;
      count_new = true;
    }
    ~guard() { count_new = false; }
  } g;
  f();
  return new_calls;
}

struct counting_resource final : std::pmr::memory_resource
{
  std::size_t allocations{};

  void* do_allocate(std::size_t bytes, std::size_t alignment) override
  {
    ++allocations;
    return std::pmr::new_delete_resource()->allocate(bytes, alignment);
  }

  void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override
  {
    std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
  }

  bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
  {
    return this == &other;
  }
};

struct default_resource_guard
{
  std::pmr::memory_resource* previous;
  explicit default_resource_guard(std::pmr::memory_resource& resource)
      : previous{std::pmr::set_default_resource(&resource)}
  {
  }
  ~default_resource_guard() { std::pmr::set_default_resource(previous); }
};
}

// Only the measured conversion is counted, not Catch's assertions or setup.
void* operator new(std::size_t size)
{
  if(count_new)
    ++new_calls;
  if(void* p = std::malloc(size ? size : 1))
    return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t size)
{
  return ::operator new(size);
}
void operator delete(void* p) noexcept
{
  std::free(p);
}
void operator delete[](void* p) noexcept
{
  std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
  std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept
{
  std::free(p);
}

TEST_CASE("allocator-aware nested maps own both sides of the conversion")
{
  using leaf = std::variant<int, std::pmr::string>;
  using map = std::map<std::pmr::string, std::vector<leaf>>;
  std::string key(160, 'k');
  std::string text(256, 'v');
  key[32] = '\0';
  text[64] = '\0';
  ossia::value encoded;
  {
    std::pmr::monotonic_buffer_resource input_storage;
    map input;
    input.emplace(
        std::pmr::string{key, &input_storage},
        std::vector<leaf>{42, std::pmr::string{text, &input_storage}});
    encoded = oscr::to_ossia_value(input);
    REQUIRE(encoded.get_type() == ossia::val_type::MAP);
  }
  // The input allocator is gone; the encoded map must still own all bytes.
  const auto* encoded_map = encoded.target<ossia::value_map_type>();
  REQUIRE(encoded_map);
  REQUIRE(encoded_map->front().first == key);
  const auto* encoded_list
      = encoded_map->front().second.target<std::vector<ossia::value>>();
  REQUIRE(encoded_list);
  REQUIRE(*(*encoded_list)[1].target<std::string>() == text);

  map decoded;
  REQUIRE(oscr::from_ossia_value(encoded, decoded));
  encoded = ossia::impulse{};
  REQUIRE(decoded.size() == 1);
  REQUIRE(std::string_view{decoded.begin()->first} == key);
  REQUIRE(std::get<int>(decoded.begin()->second[0]) == 42);
  REQUIRE(
      std::string_view{std::get<std::pmr::string>(decoded.begin()->second[1])} == text);
}

TEST_CASE("matched owning strings reuse capacity without a standard-string temporary")
{
  ossia::value source = std::string(256, 'x');
  std::string standard;
  counting_resource resource;
  std::pmr::string allocated{&resource};
  standard.reserve(512);
  allocated.reserve(512);
  const auto before = resource.allocations;
  bool ok = false;
  const auto standard_calls
      = allocations_during([&] { ok = oscr::from_ossia_value(source, standard); });
  REQUIRE(ok);
  REQUIRE(standard_calls == 0);
  const auto allocated_calls
      = allocations_during([&] { ok = oscr::from_ossia_value(source, allocated); });
  REQUIRE(ok);
  REQUIRE(allocated_calls == 0);
  REQUIRE(resource.allocations == before);
  REQUIRE(standard == *source.target<std::string>());
  REQUIRE(std::string_view{allocated} == standard);

  // Exercise the recursive decoder as well as the public string overload.
  std::vector<std::string> strings(1);
  strings[0].reserve(512);
  ossia::value list = std::vector<ossia::value>{source};
  const auto nested_calls
      = allocations_during([&] { ok = oscr::from_ossia_value(list, strings); });
  REQUIRE(ok);
  REQUIRE(nested_calls == 0);
  REQUIRE(strings[0] == standard);

  source = ossia::impulse{};
  list = ossia::impulse{};
  REQUIRE(standard == std::string(256, 'x'));
  REQUIRE(std::string_view{allocated} == standard);
  REQUIRE(strings[0] == standard);
}

TEST_CASE("native list and map decoding reuse reserved destination storage")
{
  ossia::value list = std::vector<ossia::value>{1, 2, 3};
  std::vector<ossia::value> decoded_list;
  decoded_list.reserve(16);
  bool ok = false;
  const auto list_calls
      = allocations_during([&] { ok = oscr::from_ossia_value(list, decoded_list); });
  REQUIRE(ok);
  REQUIRE(list_calls == 0);
  REQUIRE(decoded_list == *list.target<std::vector<ossia::value>>());

  ossia::value_map_type entries;
  entries.emplace_back(std::string(160, 'k'), 12);
  ossia::value map = entries;
  ossia::value_map_type decoded_map = entries;
  decoded_map.reserve(16);
  const auto map_calls
      = allocations_during([&] { ok = oscr::from_ossia_value(map, decoded_map); });
  REQUIRE(ok);
  REQUIRE(map_calls == 0);
  REQUIRE(decoded_map == entries);
  list = ossia::impulse{};
  map = ossia::impulse{};
  REQUIRE(decoded_list == std::vector<ossia::value>{1, 2, 3});
  REQUIRE(decoded_map == entries);

  // Non-native inputs retain libossia's conversion and success contracts.
  const ossia::value scalar = 7;
  REQUIRE(oscr::from_ossia_value(scalar, decoded_list));
  REQUIRE(decoded_list == ossia::convert<std::vector<ossia::value>>(scalar));
  REQUIRE(oscr::from_ossia_value(scalar, decoded_map));
  REQUIRE(decoded_map == ossia::convert<ossia::value_map_type>(scalar));
}

TEST_CASE("list append transfers converted owning strings instead of copying")
{
  counting_resource resource;
  default_resource_guard defaults{resource};
  ossia::value source
      = std::vector<ossia::value>{std::string(256, 'a'), std::string(256, 'b')};
  std::list<std::pmr::string> result;
  REQUIRE(oscr::from_ossia_value(source, result));
  REQUIRE(resource.allocations == 2);
  source = ossia::impulse{};
  REQUIRE(result.front() == std::pmr::string(256, 'a'));
  REQUIRE(result.back() == std::pmr::string(256, 'b'));
}

namespace
{
template <template <typename...> typename Variant>
void allocator_variant_roundtrip()
{
  using map = std::pmr::unordered_map<std::pmr::string, int>;
  counting_resource resource;
  Variant<int, std::pmr::string, map> result;
  result.template emplace<std::pmr::string>(&resource);
  using std::get;
  auto& string = get<std::pmr::string>(result);
  string.reserve(512);
  const auto before = resource.allocations;
  ossia::value source = std::string(256, 's');
  REQUIRE(oscr::from_ossia_value(source, result));
  REQUIRE(resource.allocations == before);
  REQUIRE(
      std::string_view{get<std::pmr::string>(result)} == *source.target<std::string>());
  REQUIRE(oscr::to_ossia_value(result) == source);

  result.template emplace<map>(&resource);
  get<map>(result).reserve(64);
  ossia::value_map_type entries;
  entries.emplace_back("one", 1);
  entries.emplace_back("two", 2);
  source = entries;
  const auto map_before = resource.allocations;
  REQUIRE(oscr::from_ossia_value(source, result));
  // Two nodes, no new bucket array: the existing map allocator is retained.
  REQUIRE(resource.allocations == map_before + 2);
  REQUIRE(get<map>(result).at(std::pmr::string{"one"}) == 1);
  REQUIRE(get<map>(result).at(std::pmr::string{"two"}) == 2);
  source = ossia::impulse{};
  REQUIRE(get<map>(result).at(std::pmr::string{"two"}) == 2);
}
}

TEST_CASE("std variants retain allocator-aware string and map storage")
{
  allocator_variant_roundtrip<std::variant>();
}

TEST_CASE("boost variants retain allocator-aware string and map storage")
{
  allocator_variant_roundtrip<boost::variant2::variant>();
}

TEST_CASE("failed conversions preserve their existing destination contracts")
{
  const ossia::value scalar = 123;
  std::string string;
  REQUIRE_FALSE(oscr::from_ossia_value(scalar, string));
  REQUIRE(string == ossia::convert<std::string>(scalar));
  std::pmr::string allocated;
  REQUIRE(oscr::from_ossia_value(scalar, allocated));
  REQUIRE(std::string_view{allocated} == string);
  std::string_view view = "old";
  REQUIRE_FALSE(oscr::from_ossia_value(scalar, view));
  REQUIRE(view.empty());
  const char* pointer = "old";
  REQUIRE_FALSE(oscr::from_ossia_value(scalar, pointer));
  REQUIRE(std::string_view{pointer}.empty());
  std::map<std::pmr::string, int> map{{std::pmr::string{"old"}, 9}};
  REQUIRE_FALSE(oscr::from_ossia_value(scalar, map));
  REQUIRE(map.at(std::pmr::string{"old"}) == 9);
  std::variant<int, std::pmr::string> variant = 17;
  REQUIRE_FALSE(oscr::from_ossia_value(ossia::value{}, variant));
  REQUIRE(std::get<int>(variant) == 17);
}

TEST_CASE("native conversion preserves snapshots across nested aliasing")
{
  using list = std::vector<ossia::value>;
  SECTION("source is inside destination")
  {
    list destination{ossia::value{list{1, 2, 3}}, 4};
    REQUIRE(oscr::from_ossia_value(destination[0], destination));
    REQUIRE(destination == list{1, 2, 3});
  }
  SECTION("source is deeply inside destination")
  {
    list destination{ossia::value{list{ossia::value{list{1, 2, 3}}}}, 4};
    auto& source = (*destination[0].target<list>())[0];
    REQUIRE(oscr::from_ossia_value(source, destination));
    REQUIRE(destination == list{1, 2, 3});
  }
  SECTION("destination is inside source")
  {
    ossia::value source{list{ossia::value{list{1, 2, 3}}, 4}};
    const auto expected = *source.target<list>();
    auto& destination = *(*source.target<list>())[0].target<list>();
    REQUIRE(oscr::from_ossia_value(source, destination));
    REQUIRE(destination == expected);
  }
  SECTION("map source is inside destination")
  {
    ossia::value_map_type inner;
    inner.emplace_back("answer", 42);
    ossia::value_map_type destination;
    destination.emplace_back("nested", ossia::value{inner});
    REQUIRE(oscr::from_ossia_value(destination.begin()->second, destination));
    REQUIRE(destination == inner);
  }
}
