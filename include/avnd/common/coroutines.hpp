#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#if __has_include(<coroutine>)
#include <coroutine>
#elif __has_include(<experimental/coroutine>)
#include <experimental/coroutine>
#else
#error No coroutine support
#endif

#if __has_include(<ranges>)
#include <ranges>
#else
namespace std
{
using namespace std::experimental;
struct default_sentinel_t
{
};
inline constexpr default_sentinel_t default_sentinel{};
}
#endif

#include <avnd/wrappers/concepts.hpp>

#include <optional>

#include <type_traits>
namespace avnd
{
// modified from https://en.cppreference.com/w/cpp/coroutine/coroutine_handle
// made for iterating one or multiple members of a structure
template <typename T>
class member_iterator
{
public:
  // Types used by the coroutine
  struct promise_type
  {
    member_iterator<T> get_return_object()
    {
      return member_iterator{handle::from_promise(*this)};
    }

    static std::suspend_always initial_suspend() noexcept { return {}; }

    static std::suspend_always final_suspend() noexcept { return {}; }

    std::suspend_always yield_value(T& value) noexcept
    {
      current_value = &value;
      return {};
    }

    void return_void() noexcept { current_value = nullptr; }

    // Disallow co_await in generator coroutines.
    void await_transform() = delete;

    [[noreturn]] static void unhandled_exception() { std::abort(); }

    T* current_value;
  };

  using handle = std::coroutine_handle<promise_type>;

  // To enable begin / end
  class iterator
  {
  public:
    explicit iterator(const handle& coroutine) noexcept
        : m_coroutine{coroutine}
    {
    }

    void operator++() noexcept { m_coroutine.resume(); }

    T& operator*() const noexcept
    {
      return *m_coroutine.promise().current_value;
    }

    bool operator==(std::default_sentinel_t) const noexcept
    {
      return !m_coroutine || m_coroutine.done();
    }

  private:
    handle m_coroutine;
  };

  // Constructors
  explicit member_iterator(handle coroutine)
      : m_coroutine{std::move(coroutine)}
  {
  }

  member_iterator() noexcept = default;
  member_iterator(const member_iterator&) = delete;
  member_iterator& operator=(const member_iterator&) = delete;

  member_iterator(member_iterator&& other) noexcept
      : m_coroutine{other.m_coroutine}
  {
    other.m_coroutine = {};
  }

  member_iterator& operator=(member_iterator&& other) noexcept
  {
    if (this != &other)
    {
      if (m_coroutine)
      {
        m_coroutine.destroy();
      }

      m_coroutine = other.m_coroutine;
      other.m_coroutine = {};
    }
    return *this;
  }

  ~member_iterator()
  {
    if (m_coroutine)
    {
      m_coroutine.destroy();
    }
  }

  // Range-based for loop support.
  iterator begin() noexcept
  {
    if (m_coroutine)
    {
      m_coroutine.resume();
    }

    return iterator{m_coroutine};
  }

  std::default_sentinel_t end() const noexcept { return {}; }

private:
  handle m_coroutine;
};

template <typename T>
class generator
{
public:
  // Types used by the coroutine
  struct promise_type
  {
    generator get_return_object()
    {
      return generator{handle::from_promise(*this)};
    }

    static std::suspend_always initial_suspend() noexcept { return {}; }

    static std::suspend_always final_suspend() noexcept { return {}; }

    std::suspend_always yield_value(T value) noexcept
    {
      current_value = value;
      return {};
    }

    void return_void() noexcept { }

    // Disallow co_await in generator coroutines.
    void await_transform() = delete;

    [[noreturn]] static void unhandled_exception() { std::abort(); }

    T current_value;
  };

  using handle = std::coroutine_handle<promise_type>;

  // To enable begin / end
  class iterator
  {
  public:
    explicit iterator(const handle& coroutine) noexcept
        : m_coroutine{coroutine}
    {
    }

    void operator++() noexcept { m_coroutine.resume(); }

    T& operator*() const noexcept
    {
      return m_coroutine.promise().current_value;
    }

    bool operator==(std::default_sentinel_t) const noexcept
    {
      return !m_coroutine || m_coroutine.done();
    }

  private:
    handle m_coroutine;
  };

  // Constructors
  explicit generator(handle coroutine)
      : m_coroutine{std::move(coroutine)}
  {
  }

  generator() noexcept = default;
  generator(const generator&) = delete;
  generator& operator=(const generator&) = delete;

  generator(generator&& other) noexcept
      : m_coroutine{other.m_coroutine}
  {
    other.m_coroutine = {};
  }

  generator& operator=(generator&& other) noexcept
  {
    if (this != &other)
    {
      if (m_coroutine)
      {
        m_coroutine.destroy();
      }

      m_coroutine = other.m_coroutine;
      other.m_coroutine = {};
    }
    return *this;
  }

  ~generator()
  {
    if (m_coroutine)
    {
      m_coroutine.destroy();
    }
  }

  // Range-based for loop support.
  iterator begin() noexcept
  {
    if (m_coroutine)
    {
      m_coroutine.resume();
    }

    return iterator{m_coroutine};
  }

  std::default_sentinel_t end() const noexcept { return {}; }

private:
  handle m_coroutine;
};
}
