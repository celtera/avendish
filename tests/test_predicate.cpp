#include <avnd/common/aggregates.hpp>
#include <boost/pfr.hpp>
#include <include/avnd/common/struct_reflection.hpp>
namespace test_fields_introspection
{
static struct Zero
{
} zero;

static struct One
{
  struct A
  {
  } a;
} one;

static struct Two
{
  struct A
  {
  } a;
  struct B
  {
  } b;
} two;

using FI0 = avnd::fields_introspection<Zero>;
using FI1 = avnd::fields_introspection<One>;
using FI2 = avnd::fields_introspection<Two>;

// Test ::type
static_assert(std::is_same_v<FI0::type, Zero>);
static_assert(std::is_same_v<FI1::type, One>);
static_assert(std::is_same_v<FI2::type, Two>);

// Test ::size
static_assert(FI0::size == 0);
static_assert(FI1::size == 1);
static_assert(FI2::size == 2);

// Test ::indices_n
static_assert(std::is_same_v<FI0::indices_n, std::integer_sequence<int>>);
static_assert(std::is_same_v<FI1::indices_n, std::integer_sequence<int, 0>>);
static_assert(std::is_same_v<FI2::indices_n, std::integer_sequence<int, 0, 1>>);
}

namespace test_predicate_introspection
{
template <typename T>
using has_foo = std::integral_constant < bool,
      requires
{
  T::foo;
}
> ;
struct GiveFoo
{
  enum
  {
    foo
  };
};
static struct Zero
{
} zero;

static struct OneA
{
  struct A : GiveFoo
  {
  } a;
} one_a;

static struct OneNone
{
  struct A
  {
  } a;
} one_n;

static struct TwoNone
{
  struct A
  {
  } a;
  struct B
  {
  } b;
} two_none;
static struct TwoA
{
  struct A : GiveFoo
  {
  } a;
  struct B
  {
  } b;
} two_a;
static struct TwoB
{
  struct A
  {
  } a;
  struct B : GiveFoo
  {
  } b;
} two_b;
static struct TwoAB
{
  struct A : GiveFoo
  {
  } a;
  struct B : GiveFoo
  {
  } b;
} two_ab;

// Check that the predicate works
static_assert(!has_foo<OneNone::A>::value);

static_assert(has_foo<OneA::A>::value);

static_assert(!has_foo<TwoNone::A>::value);
static_assert(!has_foo<TwoNone::B>::value);

static_assert(has_foo<TwoA::A>::value);
static_assert(!has_foo<TwoA::B>::value);

static_assert(!has_foo<TwoB::A>::value);
static_assert(has_foo<TwoB::B>::value);

static_assert(has_foo<TwoAB::A>::value);
static_assert(has_foo<TwoAB::B>::value);

using FI0 = avnd::predicate_introspection<Zero, has_foo>;

using FI1_N = avnd::predicate_introspection<OneNone, has_foo>;
using FI1_A = avnd::predicate_introspection<OneA, has_foo>;

using FI2_N = avnd::predicate_introspection<TwoNone, has_foo>;
using FI2_A = avnd::predicate_introspection<TwoA, has_foo>;
using FI2_B = avnd::predicate_introspection<TwoB, has_foo>;
using FI2_AB = avnd::predicate_introspection<TwoAB, has_foo>;

// Test ::type
static_assert(std::is_same_v<FI0::type, Zero>);
static_assert(std::is_same_v<FI1_N::type, OneNone>);
static_assert(std::is_same_v<FI1_A::type, OneA>);
static_assert(std::is_same_v<FI2_N::type, TwoNone>);
static_assert(std::is_same_v<FI2_A::type, TwoA>);
static_assert(std::is_same_v<FI2_B::type, TwoB>);
static_assert(std::is_same_v<FI2_AB::type, TwoAB>);

// Test ::indices
using namespace boost::mp11;
static_assert(std::is_same_v<FI0::indices, mp_list<>>);

static_assert(std::is_same_v<FI1_N::indices, mp_list<mp_int<0>>>);
static_assert(std::is_same_v<FI1_A::indices, mp_list<mp_int<0>>>);

static_assert(std::is_same_v<FI2_N::indices, mp_list<mp_int<0>, mp_int<1>>>);
static_assert(std::is_same_v<FI2_A::indices, mp_list<mp_int<0>, mp_int<1>>>);
static_assert(std::is_same_v<FI2_B::indices, mp_list<mp_int<0>, mp_int<1>>>);
static_assert(std::is_same_v<FI2_AB::indices, mp_list<mp_int<0>, mp_int<1>>>);

// Test avnd::matches_predicate
static_assert(std::is_same_v<avnd::pfr::tuple_element_t<0, OneA>, OneA::A>);
static_assert(!avnd::matches_predicate<OneNone, has_foo, mp_int<0>>::value);
static_assert(avnd::matches_predicate<OneA, has_foo, mp_int<0>>::value);

static_assert(!avnd::matches_predicate<TwoNone, has_foo, mp_int<0>>::value);
static_assert(!avnd::matches_predicate<TwoNone, has_foo, mp_int<1>>::value);
static_assert(avnd::matches_predicate<TwoA, has_foo, mp_int<0>>::value);
static_assert(!avnd::matches_predicate<TwoA, has_foo, mp_int<1>>::value);
static_assert(!avnd::matches_predicate<TwoB, has_foo, mp_int<0>>::value);
static_assert(avnd::matches_predicate<TwoB, has_foo, mp_int<1>>::value);
static_assert(avnd::matches_predicate<TwoAB, has_foo, mp_int<0>>::value);
static_assert(avnd::matches_predicate<TwoAB, has_foo, mp_int<1>>::value);

// Test ::indices_n
static_assert(std::is_same_v<FI0::indices_n, std::index_sequence<>>);

static_assert(std::is_same_v<FI1_N::indices_n, std::index_sequence<>>);
static_assert(std::is_same_v<FI1_A::indices_n, std::integer_sequence<int, 0>>);

static_assert(std::is_same_v<FI2_N::indices_n, std::index_sequence<>>);
static_assert(std::is_same_v<FI2_A::indices_n, std::integer_sequence<int, 0>>);
static_assert(std::is_same_v<FI2_B::indices_n, std::integer_sequence<int, 1>>);
static_assert(std::is_same_v<FI2_AB::indices_n, std::integer_sequence<int, 0, 1>>);

// Test ::fields
static_assert(std::is_same_v<FI0::fields, avnd::typelist<>>);

static_assert(std::is_same_v<FI1_N::fields, avnd::typelist<>>);
static_assert(std::is_same_v<FI1_A::fields, avnd::typelist<OneA::A>>);

static_assert(std::is_same_v<FI2_N::fields, avnd::typelist<>>);
static_assert(std::is_same_v<FI2_A::fields, avnd::typelist<TwoA::A>>);
static_assert(std::is_same_v<FI2_B::fields, avnd::typelist<TwoB::B>>);
static_assert(std::is_same_v<FI2_AB::fields, avnd::typelist<TwoAB::A, TwoAB::B>>);

// Test index_map
static_assert(FI0::index_map == std::array<std::size_t, 0>{});

static_assert(FI1_N::index_map == std::array<std::size_t, 0>{});

#if !defined(_GLIBCXX_DEBUG)
static_assert(FI1_A::index_map == std::array<int, 1>{0});

static_assert(FI2_N::index_map == std::array<std::size_t, 0>{});
static_assert(FI2_A::index_map == std::array<int, 1>{0});
static_assert(FI2_B::index_map == std::array<int, 1>{1});
static_assert(FI2_AB::index_map == std::array<int, 2>{0, 1});
#endif

// Test ::size
static_assert(FI0::size == 0);
static_assert(FI1_N::size == 0);
static_assert(FI1_A::size == 1);
static_assert(FI2_N::size == 0);
static_assert(FI2_A::size == 1);
static_assert(FI2_B::size == 1);
static_assert(FI2_AB::size == 2);

// Test ::field_reflections_type
static_assert(std::is_same_v<FI0::field_reflections_type, avnd::typelist<>>);

static_assert(std::is_same_v<FI1_N::field_reflections_type, avnd::typelist<>>);
static_assert(std::is_same_v<
              FI1_A::field_reflections_type,
              avnd::typelist<avnd::field_reflection<0, OneA::A>>>);

static_assert(std::is_same_v<FI2_N::field_reflections_type, avnd::typelist<>>);
static_assert(std::is_same_v<
              FI2_A::field_reflections_type,
              avnd::typelist<avnd::field_reflection<0, TwoA::A>>>);
static_assert(std::is_same_v<
              FI2_B::field_reflections_type,
              avnd::typelist<avnd::field_reflection<1, TwoB::B>>>);
static_assert(
    std::is_same_v<
        FI2_AB::field_reflections_type,
        avnd::typelist<
            avnd::field_reflection<0, TwoAB::A>, avnd::field_reflection<1, TwoAB::B>>>);

// Test ::nth_element
static_assert(std::is_same_v<typename FI1_A::nth_element<0>, OneA::A>);
static_assert(std::is_same_v<typename FI2_A::nth_element<0>, TwoA::A>);
static_assert(std::is_same_v<typename FI2_B::nth_element<0>, TwoB::B>);
static_assert(std::is_same_v<typename FI2_AB::nth_element<0>, TwoAB::A>);
static_assert(std::is_same_v<typename FI2_AB::nth_element<1>, TwoAB::B>);

static_assert(std::is_same_v<FI2_N::indices, mp_list<mp_int<0>, mp_int<1>>>);
static_assert(std::is_same_v<FI2_A::indices, mp_list<mp_int<0>, mp_int<1>>>);
static_assert(std::is_same_v<FI2_B::indices, mp_list<mp_int<0>, mp_int<1>>>);
static_assert(std::is_same_v<FI2_AB::indices, mp_list<mp_int<0>, mp_int<1>>>);
}
