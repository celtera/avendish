#pragma once
#include <avnd/common/aggregates.hpp>
#include <avnd/common/member_reflection.hpp>
#include <avnd/common/for_nth.hpp>
#include <boost/mp11/algorithm.hpp>

#include <optional>
#include <span>

#include <type_traits>

namespace avnd
{
template <typename T>
struct optionalize_all;

template <typename T>
using is_pod = std::conjunction<std::is_trivial<T>, std::is_standard_layout<T>>;

template <typename T>
constexpr bool is_pod_v = is_pod<T>::value;

template <std::size_t N>
struct ubitset
{
  static_assert(N > 0);
  struct byte
  {
    bool a : 1, b : 1, c : 1, d : 1, e : 1, f : 1, g : 1, h : 1;
  };
  byte enabled[N / 8 + (N % 8 != 0)];

  // Constructor
  static constexpr ubitset zeroed() noexcept { return {.enabled = {}}; }

  // Utility
  static consteval std::size_t size() noexcept { return N; }

  constexpr auto quo_rem(std::size_t idx) const noexcept
  {
    struct
    {
      std::size_t quo, rem;
    } r{idx / 8, idx % 8};
    return r;
  }

  constexpr bool test(std::size_t idx) const noexcept
  {
    const auto [quo, rem] = quo_rem(idx);
    const auto b = enabled[quo];
    switch (rem)
    {
      case 0: return b.a;
      case 1: return b.b;
      case 2: return b.c;
      case 3: return b.d;
      case 4: return b.e;
      case 5: return b.f;
      case 6: return b.g;
      case 7: return b.h;
      default: return 0;
    }
  }

  constexpr void set(std::size_t idx, bool bo) noexcept
  {
    const auto [quo, rem] = quo_rem(idx);
    auto& b = enabled[quo];
    switch (rem)
    {
      case 0: b.a = bo; break;
      case 1: b.b = bo; break;
      case 2: b.c = bo; break;
      case 3: b.d = bo; break;
      case 4: b.e = bo; break;
      case 5: b.f = bo; break;
      case 6: b.g = bo; break;
      case 7: b.h = bo; break;
    }
  }
};

template <typename T>
requires(is_pod_v<T>) struct optionalize_all<T>
{
  static constexpr std::size_t member_count = avnd::pfr::tuple_size_v<T>;
  T m_storage;
  ubitset<member_count> m_engaged;

  static constexpr optionalize_all zeroed() noexcept
  {
    // Note that m_storage won't be zeroed - we
    // only care about m_engaged
    return {
            .m_storage = { },
            .m_engaged = {
                      .enabled = {}
        }
    };
  }

  template <auto member>
  constexpr bool engaged() const noexcept
  {
    constexpr auto idx = avnd::index_in_struct(T{}, member);
    return m_engaged.test(idx);
  }

  template <auto member>
  constexpr auto get() noexcept
  {
    using ret = avnd::member_type<member>;
    constexpr auto idx = index_in_struct(T{}, member);
    return m_engaged.test(idx) ? std::optional<ret>{m_storage.*member} : std::nullopt;
  }

  template <auto member, typename U>
  constexpr void set(U&& value) noexcept
  {
    constexpr auto idx = index_in_struct(T{}, member);

    static_assert(noexcept(m_storage.*member = std::forward<U>(value)));
    m_storage.*member = std::forward<U>(value);
    m_engaged.set(idx, true);
  }

  template <auto member>
  constexpr void set(std::nullopt_t) noexcept
  {
    constexpr auto idx = index_in_struct(T{}, member);
    m_engaged.set(idx, false);
  }
};

template <typename T>
struct optionalize_all
{
  static constexpr std::size_t member_count = avnd::pfr::tuple_size_v<T>;
  alignas(T) char m_storage[sizeof(T)];
  ubitset<member_count> m_engaged = ubitset<member_count>::zeroed();

  optionalize_all() = default;

  ~optionalize_all()
  {
    std::size_t i = 0;
      avnd::pfr::for_each_field(
          *reinterpret_cast<T*>(m_storage),
          [&]<typename F>(F& field)
          {
            if constexpr (!is_pod_v<F>)
              if (m_engaged.test(i))
                std::destroy_at(&field);
            i++;
          });
  }

  optionalize_all(const optionalize_all& other)
      : m_engaged{other.m_engaged}
  {
    std::size_t i = 0;
      avnd::pfr::for_each_field(
          *reinterpret_cast<T*>(m_storage),
          [&]<typename F>(F& field)
          {
            if (other.m_engaged.test(i))
            {
              const std::ptrdiff_t offset = (char*)(&field) - m_storage;
              std::construct_at(
                  &field, *reinterpret_cast<const F*>(other.m_storage + offset));
            }
            i++;
          });
  }

  optionalize_all(optionalize_all&& other) noexcept
      : m_engaged{other.m_engaged}
  {
    std::size_t i = 0;
      avnd::pfr::for_each_field(
          *reinterpret_cast<T*>(m_storage),
          [&]<typename F>(F& field)
          {
            if (other.m_engaged.test(i))
            {
              const std::ptrdiff_t offset = (char*)(&field) - m_storage;
              std::construct_at(
                  &field, std::move(*reinterpret_cast<F*>(other.m_storage + offset)));
            }
            i++;
          });
  }

  optionalize_all& operator=(const optionalize_all& other)
  {
    std::size_t i = 0;
      avnd::pfr::for_each_field(
          *reinterpret_cast<T*>(m_storage),
          [&]<typename F>(F& field)
          {
            if (m_engaged.test(i) && other.m_engaged.test(i))
            {
              const std::ptrdiff_t offset = (char*)(&field) - m_storage;
              field = *reinterpret_cast<const F*>(other.m_storage + offset);
            }
            else if (m_engaged.test(i) && !other.m_engaged.test(i))
            {
              std::destroy_at(&field);
              m_engaged.set(i, false);
            }
            else if (!m_engaged.test(i) && other.m_engaged.test(i))
            {
              const std::ptrdiff_t offset = (char*)(&field) - m_storage;
              std::construct_at(
                  &field, *reinterpret_cast<const F*>(other.m_storage + offset));
              m_engaged.set(i, true);
            }
            // Final case where none are engaged: do nothing
            i++;
          });
    return *this;
  }

  optionalize_all& operator=(optionalize_all&& other) noexcept
  {
    std::size_t i = 0;
      avnd::pfr::for_each_field(
          *reinterpret_cast<T*>(m_storage),
          [&]<typename F>(F& field)
          {
            if (m_engaged.test(i) && other.m_engaged.test(i))
            {
              const std::ptrdiff_t offset = (char*)(&field) - m_storage;
              field = std::move(*reinterpret_cast<const F*>(other.m_storage + offset));
            }
            else if (m_engaged.test(i) && !other.m_engaged.test(i))
            {
              std::destroy_at(&field);
              m_engaged.set(i, false);
            }
            else if (!m_engaged.test(i) && other.m_engaged.test(i))
            {
              const std::ptrdiff_t offset = (char*)(&field) - m_storage;
              std::construct_at(
                  &field,
                  std::move(*reinterpret_cast<const F*>(other.m_storage + offset)));
              m_engaged.set(i, true);
            }
            // Final case where none are engaged: do nothing
            i++;
          });
    return *this;
  }

  template <auto member>
  constexpr bool engaged() const noexcept
  {
    constexpr auto idx = index_in_struct(T{}, member);
    return m_engaged.test(idx);
  }

  template <auto member>
  constexpr auto get()
  {
    using ret = avnd::member_type<member>;
    constexpr auto idx = index_in_struct(T{}, member);
    return m_engaged.test(idx)
               ? std::optional<ret>{reinterpret_cast<T*>(m_storage)->*member}
               : std::nullopt;
  }

  template <auto member, typename U>
  constexpr void set(U&& value)
  {
    using ret = avnd::member_type<member>;
    constexpr auto idx = index_in_struct(T{}, member);

    auto& mem = (reinterpret_cast<T*>(m_storage)->*member);
    if constexpr (is_pod_v<ret>)
    {
      mem = std::forward<U>(value);
      m_engaged.set(idx, true);
    }
    else
    {
      if (!m_engaged.test(idx))
      {
        std::construct_at(&mem, std::forward<U>(value));
        m_engaged.set(idx, true);
      }
      else
      {
        mem = std::forward<U>(value);
      }
    }
  }

  template <auto member>
  constexpr void set(std::nullopt_t)
  {
    using ret = avnd::member_type<member>;
    constexpr auto idx = index_in_struct(T{}, member);
    if constexpr (!is_pod_v<ret>)
    {
      if (m_engaged.test(idx))
        std::destroy_at(reinterpret_cast<T*>(m_storage)->*member);
    }

    m_engaged.set(idx, false);
  }
};

}

/*
struct large_test {
  int a,b,c,d,e,f,g,h;
  float aa,ab,ac,ad,ae,af,ag,ah;
  char ba,bb,bc,bd,be,bf,bg,bh;
};
static_assert(sizeof(avnd::optionalize_all<large_test>) == sizeof(large_test) + 4);

// POD case can run in a constexpr context
constexpr int res = [] () constexpr {
  using namespace avnd;
  struct foo { int a; float b; char c; };

  using opt_foo = avnd::optionalize_all<foo>;
  // if foo is pod, opt_foo is also one, thus nothing is initialized, be careful
  static_assert(is_pod_v<foo>);
  static_assert(is_pod_v<opt_foo>);

  opt_foo opt = opt_foo::zeroed();
  assert(!opt.engaged<&foo::a>());
  assert(!opt.engaged<&foo::b>());
  assert(!opt.engaged<&foo::c>());

  assert(opt.get<&foo::a>() == std::nullopt);
  assert(opt.get<&foo::b>() == std::nullopt);
  assert(opt.get<&foo::c>() == std::nullopt);

  opt.set<&foo::b>(123);

  assert(opt.engaged<&foo::b>());
  assert(*opt.get<&foo::b>() == 123.f);

  return 0;
}();

int res2 = [] () {
  using namespace avnd;
  struct foo { int a; std::vector<float> b; char c; std::string d; };

  using opt_foo = avnd::optionalize_all<foo>;
  static_assert(!is_pod_v<foo>);
  static_assert(!is_pod_v<opt_foo>);

  opt_foo opt;
  assert(!opt.engaged<&foo::a>());
  assert(!opt.engaged<&foo::b>());
  assert(!opt.engaged<&foo::c>());

  assert(opt.get<&foo::a>() == std::nullopt);
  assert(opt.get<&foo::b>() == std::nullopt);
  assert(opt.get<&foo::c>() == std::nullopt);
  assert(opt.get<&foo::d>() == std::nullopt);

  std::vector<float> test_vec{1.f, 2.f, 3.f};
  opt.set<&foo::b>(test_vec);
  opt.set<&foo::c>(44);

  assert(!opt.engaged<&foo::a>());
  assert(opt.engaged<&foo::b>());
  assert(opt.engaged<&foo::c>());
  assert(!opt.engaged<&foo::d>());
  assert(opt.get<&foo::a>() == std::nullopt);
  assert(*opt.get<&foo::b>() == test_vec);
  assert(*opt.get<&foo::c>() == 44);
  assert(opt.get<&foo::d>() == std::nullopt);

  {
    auto copy = opt;
    assert(opt.get<&foo::a>() == std::nullopt);
    assert(opt.get<&foo::b>() == test_vec);
    assert(opt.get<&foo::c>() == 44);
    assert(opt.get<&foo::d>() == std::nullopt);

    assert(copy.get<&foo::a>() == std::nullopt);
    assert(copy.get<&foo::b>() == test_vec);
    assert(copy.get<&foo::c>() == 44);
    assert(copy.get<&foo::d>() == std::nullopt);
  }

  {
    auto move = std::move(opt);
    assert(opt.get<&foo::a>() == std::nullopt);
    assert(opt.get<&foo::b>() == std::vector<float>{});
    assert(opt.get<&foo::c>() == 44);
    assert(opt.get<&foo::d>() == std::nullopt);

    assert(move.get<&foo::a>() == std::nullopt);
    assert(move.get<&foo::b>() == test_vec);
    assert(move.get<&foo::c>() == 44);
    assert(move.get<&foo::d>() == std::nullopt);
  }

  return 0;
}();

*/
