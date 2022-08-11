#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>
#include <array>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>

void test()
{
  using namespace avnd;
  using namespace oscr;
  auto test = [] <typename T> (T v){
    ossia::value val = to_ossia_value(v);
    T t;
    SCORE_ASSERT(from_ossia_value(val, t));
    SCORE_ASSERT(t == v);
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

  struct Agg {
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
  test.operator()<Agg>(Agg{1, 0.5f, true, 'x', "hello", {1, 3, 5}, {6,7}, std::bitset<64>{1234ULL}, {1, -1}, { {1, 2}, {3, 4}}, {"foo", "bar"}, {{ 1, "foo"}, {3, "bar"}}});
}
