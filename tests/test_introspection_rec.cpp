#include <avnd/common/aggregates.hpp>
#include <boost/pfr.hpp>
#define REC         \
  enum              \
  {                 \
    recursive_group \
  }

namespace test_1
{
static struct Zero
{
} zero;

static struct OneRec
{
  struct A
  {
    REC;
    struct B
    {
    } b;
  } a;
} one;

static struct TwoRec
{
  struct A
  {
    REC;
    struct B
    {
    } b;
  } a;
  struct C
  {
  } c;
} two;

static_assert(avnd::pfr::tuple_size_v<Zero> == 0);

static_assert(avnd::pfr::tuple_size_v<OneRec> == 1);

static_assert(avnd::pfr::tuple_size_v<TwoRec> == 2);

static_assert(std::is_same_v<decltype(avnd::pfr::get<0>(one)), OneRec::A::B&>);
static_assert(std::is_same_v<decltype(avnd::pfr::get<0>(two)), TwoRec::A::B&>);
static_assert(std::is_same_v<decltype(avnd::pfr::get<1>(two)), TwoRec::C&>);

static auto tpl_zero = avnd::pfr::detail::tie_as_tuple(zero);
static auto tpl_one = avnd::pfr::detail::tie_as_tuple(one);
static auto tpl_two = avnd::pfr::detail::tie_as_tuple(two);

// static_assert(tuple_size_v<decltype(tpl_zero)> == 0);
// static_assert(tuple_size_v<decltype(tpl_one)> == 1);
// static_assert(tuple_size_v<decltype(tpl_two)> == 2);
static_assert(std::is_same_v<decltype(get<0>(tpl_one)), OneRec::A::B&>);
static_assert(std::is_same_v<decltype(get<0>(tpl_two)), TwoRec::A::B&>);
static_assert(std::is_same_v<decltype(get<1>(tpl_two)), TwoRec::C&>);

static auto tl_zero = avnd::as_typelist<decltype(zero)>{};
static auto tl_one = avnd::as_typelist<decltype(one)>{};
static auto tl_two = avnd::as_typelist<decltype(two)>{};

static_assert(boost::mp11::mp_size<decltype(tl_zero)>::value == 0);
static_assert(boost::mp11::mp_size<decltype(tl_one)>::value == 1);
static_assert(boost::mp11::mp_size<decltype(tl_two)>::value == 2);

static_assert(
    std::is_same_v<
        boost::mp11::mp_at<decltype(tl_one), boost::mp11::mp_int<0>>, OneRec::A::B>);
static_assert(
    std::is_same_v<
        boost::mp11::mp_at<decltype(tl_two), boost::mp11::mp_int<0>>, TwoRec::A::B>);
static_assert(std::is_same_v<
              boost::mp11::mp_at<decltype(tl_two), boost::mp11::mp_int<1>>, TwoRec::C>);

static_assert(boost::mp11::mp_size<avnd::as_typelist<Zero>>::value == 0);
static_assert(boost::mp11::mp_size<avnd::as_typelist<OneRec>>::value == 1);
static_assert(boost::mp11::mp_size<avnd::as_typelist<TwoRec>>::value == 2);

static_assert(std::is_same_v<
              boost::mp11::mp_at<avnd::as_typelist<OneRec>, boost::mp11::mp_int<0>>,
              OneRec::A::B>);
static_assert(std::is_same_v<
              boost::mp11::mp_at<avnd::as_typelist<TwoRec>, boost::mp11::mp_int<0>>,
              TwoRec::A::B>);
static_assert(std::is_same_v<
              boost::mp11::mp_at<avnd::as_typelist<TwoRec>, boost::mp11::mp_int<1>>,
              TwoRec::C>);
}
