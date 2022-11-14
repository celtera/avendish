#include <avnd/common/aggregates.hpp>
#include <boost/pfr.hpp>
namespace test_1
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

static_assert(boost::pfr::detail::fields_count<Zero>() == 0);
static_assert(boost::pfr::tuple_size_v<Zero> == 0);

static_assert(avnd::pfr::tuple_size_v<Zero> == 0);

static_assert(boost::pfr::detail::fields_count<One>() == 1);
static_assert(boost::pfr::tuple_size_v<One> == 1);

static_assert(avnd::pfr::tuple_size_v<One> == 1);

static_assert(boost::pfr::detail::fields_count<Two>() == 2);
static_assert(boost::pfr::tuple_size_v<Two> == 2);

static_assert(avnd::pfr::tuple_size_v<Two> == 2);

static_assert(std::is_same_v<decltype(avnd::pfr::get<0>(one)), One::A&>);
static_assert(std::is_same_v<decltype(avnd::pfr::get<0>(two)), Two::A&>);
static_assert(std::is_same_v<decltype(avnd::pfr::get<1>(two)), Two::B&>);

static auto tpl_zero = avnd::pfr::detail::tie_as_tuple(zero);
static auto tpl_one = avnd::pfr::detail::tie_as_tuple(one);
static auto tpl_two = avnd::pfr::detail::tie_as_tuple(two);

// static_assert(tuple_size_v<decltype(tpl_zero)> == 0);
// static_assert(tuple_size_v<decltype(tpl_one)> == 1);
// static_assert(tuple_size_v<decltype(tpl_two)> == 2);
static_assert(std::is_same_v<decltype(get<0>(tpl_one)), One::A&>);
static_assert(std::is_same_v<decltype(get<0>(tpl_two)), Two::A&>);
static_assert(std::is_same_v<decltype(get<1>(tpl_two)), Two::B&>);

static auto tl_zero = avnd::as_typelist<decltype(zero)>{};
static auto tl_one = avnd::as_typelist<decltype(one)>{};
static auto tl_two = avnd::as_typelist<decltype(two)>{};

static_assert(boost::mp11::mp_size<decltype(tl_zero)>::value == 0);
static_assert(boost::mp11::mp_size<decltype(tl_one)>::value == 1);
static_assert(boost::mp11::mp_size<decltype(tl_two)>::value == 2);

static_assert(std::is_same_v<
              boost::mp11::mp_at<decltype(tl_one), boost::mp11::mp_int<0>>, One::A>);
static_assert(std::is_same_v<
              boost::mp11::mp_at<decltype(tl_two), boost::mp11::mp_int<0>>, Two::A>);
static_assert(std::is_same_v<
              boost::mp11::mp_at<decltype(tl_two), boost::mp11::mp_int<1>>, Two::B>);

static_assert(boost::mp11::mp_size<avnd::as_typelist<Zero>>::value == 0);
static_assert(boost::mp11::mp_size<avnd::as_typelist<One>>::value == 1);
static_assert(boost::mp11::mp_size<avnd::as_typelist<Two>>::value == 2);

static_assert(
    std::is_same_v<
        boost::mp11::mp_at<avnd::as_typelist<One>, boost::mp11::mp_int<0>>, One::A>);
static_assert(
    std::is_same_v<
        boost::mp11::mp_at<avnd::as_typelist<Two>, boost::mp11::mp_int<0>>, Two::A>);
static_assert(
    std::is_same_v<
        boost::mp11::mp_at<avnd::as_typelist<Two>, boost::mp11::mp_int<1>>, Two::B>);
}

namespace test_2
{
static struct Zero
{
} zero;

static struct One
{
  struct A
  {
    float x;
  } a;
} one;

static struct Two
{
  struct A
  {
    float x;
  } a;
  struct B
  {
    float y;
  } b;
} two;

static_assert(boost::pfr::detail::fields_count<Zero>() == 0);
static_assert(boost::pfr::tuple_size_v<Zero> == 0);

static_assert(avnd::pfr::tuple_size_v<Zero> == 0);

static_assert(boost::pfr::detail::fields_count<One>() == 1);
static_assert(boost::pfr::tuple_size_v<One> == 1);

static_assert(avnd::pfr::tuple_size_v<One> == 1);

static_assert(boost::pfr::detail::fields_count<Two>() == 2);
static_assert(boost::pfr::tuple_size_v<Two> == 2);

static_assert(avnd::pfr::tuple_size_v<Two> == 2);

static_assert(std::is_same_v<decltype(avnd::pfr::get<0>(one)), One::A&>);
static_assert(std::is_same_v<decltype(avnd::pfr::get<0>(two)), Two::A&>);
static_assert(std::is_same_v<decltype(avnd::pfr::get<1>(two)), Two::B&>);

static auto tpl_zero = avnd::pfr::detail::tie_as_tuple(zero);
static auto tpl_one = avnd::pfr::detail::tie_as_tuple(one);
static auto tpl_two = avnd::pfr::detail::tie_as_tuple(two);

// static_assert(std::tuple_size_v<decltype(tpl_zero)> == 0);
// static_assert(std::tuple_size_v<decltype(tpl_one)> == 1);
// static_assert(std::tuple_size_v<decltype(tpl_two)> == 2);
static_assert(std::is_same_v<decltype(get<0>(tpl_one)), One::A&>);
static_assert(std::is_same_v<decltype(get<0>(tpl_two)), Two::A&>);
static_assert(std::is_same_v<decltype(get<1>(tpl_two)), Two::B&>);

static auto tl_zero = avnd::as_typelist<decltype(zero)>{};
static auto tl_one = avnd::as_typelist<decltype(one)>{};
static auto tl_two = avnd::as_typelist<decltype(two)>{};

static_assert(boost::mp11::mp_size<decltype(tl_zero)>::value == 0);
static_assert(boost::mp11::mp_size<decltype(tl_one)>::value == 1);
static_assert(boost::mp11::mp_size<decltype(tl_two)>::value == 2);

static_assert(std::is_same_v<
              boost::mp11::mp_at<decltype(tl_one), boost::mp11::mp_int<0>>, One::A>);
static_assert(std::is_same_v<
              boost::mp11::mp_at<decltype(tl_two), boost::mp11::mp_int<0>>, Two::A>);
static_assert(std::is_same_v<
              boost::mp11::mp_at<decltype(tl_two), boost::mp11::mp_int<1>>, Two::B>);

static_assert(boost::mp11::mp_size<avnd::as_typelist<Zero>>::value == 0);
static_assert(boost::mp11::mp_size<avnd::as_typelist<One>>::value == 1);
static_assert(boost::mp11::mp_size<avnd::as_typelist<Two>>::value == 2);

static_assert(
    std::is_same_v<
        boost::mp11::mp_at<avnd::as_typelist<One>, boost::mp11::mp_int<0>>, One::A>);
static_assert(
    std::is_same_v<
        boost::mp11::mp_at<avnd::as_typelist<Two>, boost::mp11::mp_int<0>>, Two::A>);
static_assert(
    std::is_same_v<
        boost::mp11::mp_at<avnd::as_typelist<Two>, boost::mp11::mp_int<1>>, Two::B>);
}
