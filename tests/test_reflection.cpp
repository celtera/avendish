/* SPDX-License-Identifier: GPL-3.0-or-later */

// Extensive validation of the C++26 static-reflection backends. The whole body
// is guarded on AVND_USE_STD_REFLECTION so non-reflecting compilers (the CI
// boost.pfr / p1061 builds) compile a trivial program, while reflecting
// compilers (GCC 16 -freflection, clang-p2996 -freflection-latest) exercise:
//   1. aggregates flattening (recursive_group + raw port arrays)
//   2. enum reflection (avnd::enum_*)
//   3. function_reflection
//   4. field names
//   5. [[=halp::meta(...)]] metadata
//   6. [[=halp::message]] annotated messages
//
// Run as an executable: returns 0 on success, non-zero (and prints) on failure.

#include <avnd/common/meta_polyfill.hpp>

#include <cstdio>

#if !AVND_USE_STD_REFLECTION
int main()
{
  std::puts("test_reflection: skipped (no C++26 reflection)");
  return 0;
}
#else

#include <avnd/common/enum_reflection.hpp>
#include <avnd/common/function_reflection.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/callback.hpp>
#include <avnd/concepts/message.hpp>
#include <avnd/introspection/field_names.hpp>
#include <avnd/introspection/messages.p2996.hpp>
#include <avnd/introspection/metadata.p2996.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <halp/messages.hpp>
#include <halp/meta.hpp>

#include <string>
#include <string_view>
#include <type_traits>

static int g_failures = 0;
#define CHECK(cond)                                                  \
  do                                                                 \
  {                                                                  \
    if(!(cond))                                                      \
    {                                                                \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
      ++g_failures;                                                  \
    }                                                                \
  } while(0)

using namespace std::string_view_literals;

// ============================================================================
// 1. AGGREGATES FLATTENING
// ============================================================================
namespace agg
{
struct Port
{
  int value{};
};
struct Other
{
  float v{};
};

// recursive_group: leaves inlined into the parent
struct Group
{
  enum
  {
    recursive_group
  };
  Port a, b, c;
};
// a group nesting another group: 1 + (group of 3) = 4 leaves
struct NestedGroup
{
  enum
  {
    recursive_group
  };
  Port head;
  Group inner;
};

struct Flat
{
  Port x;
  Other y;
}; // 2
struct OneGroup
{
  Port head;
  Group g;
  Port tail;
}; // 1 + 3 + 1 = 5
struct TwoGroups
{
  Group g1;
  Group g2;
}; // 6
struct DeepNest
{
  Port a;
  NestedGroup n;
}; // 1 + 4 = 5
struct PortArray
{
  Port voices[8];
}; // 8
struct GroupArray
{
  Port head;
  Group drums[4];
}; // 1 + 4*3 = 13
struct Mixed
{
  Port lead;
  Group fx;
  Other knobs[3];
  Group banks[2];
}; // 1 + 3 + 3 + 6 = 13

// edge cases
struct Empty
{
}; // 0
struct Single
{
  Port only;
}; // 1
struct ArrayOfOne
{
  Group g[1];
}; // 3
struct GroupWithArray
{
  enum
  {
    recursive_group
  };
  Port head;
  Other tones[4];
}; // 1 + 4 = 5 (array inside a group)
struct ArrayOfNestedGroups
{
  NestedGroup banks[2];
}; // 2 * 4 = 8

template <typename T>
using FI = avnd::fields_introspection<T>;

// --- compile-time leaf counts ---
static_assert(FI<Flat>::size == 2);
static_assert(FI<OneGroup>::size == 5);
static_assert(FI<TwoGroups>::size == 6);
static_assert(FI<DeepNest>::size == 5);
static_assert(FI<PortArray>::size == 8);
static_assert(FI<GroupArray>::size == 13);
static_assert(FI<Mixed>::size == 13);
static_assert(FI<Empty>::size == 0);
static_assert(FI<Single>::size == 1);
static_assert(FI<ArrayOfOne>::size == 3);
static_assert(FI<GroupWithArray>::size == 5);
static_assert(FI<ArrayOfNestedGroups>::size == 8);

// --- flattened leaf TYPES (get<N> / tuple_element_t / as_typelist) ---
static_assert(std::is_same_v<decltype(avnd::pfr::get<0>(std::declval<OneGroup&>())), Port&>);
static_assert(std::is_same_v<decltype(avnd::pfr::get<1>(std::declval<OneGroup&>())), Port&>); // g.a
static_assert(std::is_same_v<decltype(avnd::pfr::get<4>(std::declval<OneGroup&>())), Port&>); // tail
static_assert(avnd::pfr::tuple_size_v<GroupArray> == 13);
static_assert(
    std::is_same_v<avnd::pfr::tuple_element_t<0, Mixed>, Port>); // lead
static_assert(
    std::is_same_v<avnd::pfr::tuple_element_t<4, Mixed>, Other>); // first knobs[]

// as_typelist length matches the flattened leaf count
static_assert(boost::mp11::mp_size<avnd::as_typelist<GroupArray>>::value == 13);
static_assert(boost::mp11::mp_size<avnd::as_typelist<Mixed>>::value == 13);

void run()
{
  // runtime: assign a distinct value to each leaf, then read it back through
  // the *nested* member path to prove tie targets the real subobjects.
  GroupArray obj{};
  int i = 0;
  FI<GroupArray>::for_all(obj, [&](auto& port) { port.value = ++i; });
  CHECK(i == 13);
  CHECK(obj.head.value == 1);
  CHECK(obj.drums[0].a.value == 2);
  CHECK(obj.drums[0].b.value == 3);
  CHECK(obj.drums[3].c.value == 13);

  // sum via flattened iteration == 1+..+13
  int sum = 0;
  FI<GroupArray>::for_all(obj, [&](auto& p) { sum += p.value; });
  CHECK(sum == 91);

  // mutate via get<N> and observe through the nested path
  avnd::pfr::get<2>(obj).value = 999; // drums[0].b
  CHECK(obj.drums[0].b.value == 999);

  // deep nesting
  DeepNest dn{};
  int k = 0;
  FI<DeepNest>::for_all(dn, [&](auto& p) { p.value = ++k; });
  CHECK(k == 5);
  CHECK(dn.a.value == 1);
  CHECK(dn.n.head.value == 2);
  CHECK(dn.n.inner.a.value == 3);
  CHECK(dn.n.inner.c.value == 5);

  // flat parity: a struct with no groups/arrays behaves like direct members
  Flat f{};
  int n = 0;
  FI<Flat>::for_all(f, [&](auto&) { ++n; });
  CHECK(n == 2);
}
}

// ============================================================================
// 2. ENUM REFLECTION
// ============================================================================
namespace en
{
enum class Wave
{
  Sine,
  Square,
  Saw,
  Triangle,
  Noise
};

static_assert(avnd::enum_count<Wave>() == 5);
static_assert(avnd::enum_names<Wave>()[0] == "Sine"sv);
static_assert(avnd::enum_names<Wave>()[2] == "Saw"sv);
static_assert(avnd::enum_values<Wave>()[3] == Wave::Triangle);
static_assert(avnd::enum_underlying(Wave::Noise) == 4);
static_assert(avnd::enum_entries<Wave>()[1].first == Wave::Square);
static_assert(avnd::enum_entries<Wave>()[1].second == "Square"sv);

void run()
{
  CHECK(avnd::enum_name(Wave::Saw) == "Saw"sv);
  CHECK(avnd::enum_cast<Wave>("Triangle").has_value());
  CHECK(*avnd::enum_cast<Wave>("Triangle") == Wave::Triangle);
  CHECK(!avnd::enum_cast<Wave>("Nope").has_value());

  // name().data() must be a valid NUL-terminated C string
  auto nm = avnd::enum_names<Wave>()[2];
  CHECK(std::char_traits<char>::length(nm.data()) == 3);
  CHECK(nm.data()[3] == '\0');
}
}

// ============================================================================
// 3. FUNCTION REFLECTION
// ============================================================================
namespace fr
{
int free_fn(double, char) noexcept;
struct C
{
  void m(int) const;
  float* g(const char*, C&) noexcept;
};

static_assert(avnd::function_reflection<free_fn>::count == 2);
static_assert(avnd::function_reflection<free_fn>::is_noexcept);
static_assert(
    std::is_same_v<avnd::function_reflection<free_fn>::return_type, int>);
static_assert(std::is_same_v<
              boost::mp11::mp_first<avnd::function_reflection<free_fn>::arguments>,
              double>);

static_assert(avnd::function_reflection<&C::m>::count == 1);
static_assert(avnd::function_reflection<&C::m>::is_const);
static_assert(!avnd::function_reflection<&C::m>::is_noexcept);
static_assert(std::is_same_v<avnd::function_reflection<&C::m>::class_type, C>);

static_assert(avnd::function_reflection<&C::g>::is_noexcept);
static_assert(!avnd::function_reflection<&C::g>::is_const);
static_assert(
    std::is_same_v<avnd::function_reflection<&C::g>::return_type, float*>);
static_assert(std::is_same_v<
              avnd::function_reflection<&C::g>::arguments,
              boost::mp11::mp_list<const char*, C&>>);
}

// ============================================================================
// 4. FIELD NAMES
// ============================================================================
namespace fn
{
struct Plain
{
  float cutoff;
  double resonance;
  int kind;
};

void run()
{
  auto names = avnd::get_field_names<Plain>();
  CHECK(names.size() == 3);
  CHECK(names[0] == "cutoff"sv);
  CHECK(names[1] == "resonance"sv);
  CHECK(names[2] == "kind"sv);
  CHECK(names[1].data()[names[1].size()] == '\0'); // NUL-terminated
}
}

// ============================================================================
// 5. METADATA ANNOTATIONS
// ============================================================================
namespace md
{
struct [[=halp::meta("author", "JM Celerier")]] [[=halp::meta("category", "Filters")]] [[=halp::meta("name", "My Filter")]] Tagged
{
};

static_assert(avnd::annotated_metadata<Tagged>("author") == "JM Celerier"sv);
static_assert(avnd::annotated_metadata<Tagged>("category") == "Filters"sv);
static_assert(avnd::annotated_metadata<Tagged>("missing").empty());

// integrated through the real metadata getters
static_assert(avnd::get_name<Tagged>() == "My Filter"sv);
static_assert(avnd::get_category<Tagged>() == "Filters"sv);
}

// ============================================================================
// 6. ANNOTATED MESSAGES
// ============================================================================
namespace ms
{
struct Obj
{
  int counter = 0;
  [[=halp::message]] void increment(int by) { counter += by; }
  [[=halp::message]] void reset() { counter = 0; }
  [[=halp::message_named("set!")]] void assign(int v) { counter = v; }
  void not_a_message() { }
};

using Intro = avnd::annotated_messages_introspection<Obj>;
static_assert(Intro::size == 3);

void run()
{
  Obj obj;
  int seen = 0;
  bool saw_set = false, saw_inc = false;
  Intro::for_all([&]<typename Field>(Field) {
    using M = typename Field::type;
    static_assert(avnd::stateless_message<M>);
    ++seen;
    if(M::name() == "set!"sv)
      saw_set = true;
    if(M::name() == "increment"sv)
      saw_inc = true;
  });
  CHECK(seen == 3);
  CHECK(saw_set);
  CHECK(saw_inc);

  // invoke increment via its reflected member pointer
  Intro::for_all([&]<typename Field>(Field) {
    using M = typename Field::type;
    if constexpr(M::name() == "increment"sv)
    {
      constexpr auto f = avnd::message_get_func<M>();
      using refl = avnd::message_reflection<M>;
      static_assert(refl::count == 1);
      (obj.*f)(40);
    }
  });
  obj.increment(2);
  CHECK(obj.counter == 42);
}
}

int main()
{
  agg::run();
  en::run();
  fn::run();
  ms::run();

  if(g_failures == 0)
    std::puts("test_reflection: ALL PASS");
  else
    std::printf("test_reflection: %d FAILURE(S)\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}

#endif
