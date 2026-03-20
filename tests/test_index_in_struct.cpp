#include <avnd/common/for_nth.hpp>

#if !defined(_MSC_VER)
namespace test1
{
struct Foo
{
  int a;
  float b;
  struct X
  {
    char c;
  } x;
};

static_assert(avnd::index_in_struct(Foo{}, &Foo::a) == 0);
static_assert(avnd::index_in_struct(Foo{}, &Foo::b) == 1);
static_assert(avnd::index_in_struct(Foo{}, &Foo::x) == 2);
}

namespace test2
{
struct Foo
{
  int a;
  float b;
  struct X
  {
    enum
    {
      recursive_group
    };
    char c;
  } x;
};

static_assert(avnd::index_in_struct(Foo{}, &Foo::a) == 0);
static_assert(avnd::index_in_struct(Foo{}, &Foo::b) == 1);
static_assert(avnd::index_in_struct(Foo{}, &Foo::x, &Foo::X::c) == 2);
}

#endif