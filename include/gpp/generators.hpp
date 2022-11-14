#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/common/coroutines.hpp>
#include <utility>

// std::generator is not implemented yet, so polyfill it more or less
namespace gpp
{
template <typename Out, typename In>
class generator
{
public:
  // Types used by the coroutine
  struct promise_type
  {
    Out current_command;
    In feedback_value;

    template <typename Ret>
    struct awaiter : std::suspend_always
    {
      friend promise_type;
      constexpr auto await_resume() const { return get<Ret>(p.feedback_value); }

      promise_type& p;
    };

    generator get_return_object() { return generator{handle::from_promise(*this)}; }

    static std::suspend_always initial_suspend() noexcept { return {}; }

    static std::suspend_always final_suspend() noexcept { return {}; }

    template <typename T>
    auto yield_value(T value) noexcept
    {
      current_command = value;
      using ret = typename T::return_type;
      if constexpr(std::is_same_v<ret, void>)
        return std::suspend_always{};
      else
        return awaiter<ret>{{}, *this};
    }

    void return_void() noexcept { }

    // Disallow co_await in generator coroutines.
    void await_transform() = delete;

    [[noreturn]] static void unhandled_exception() { std::abort(); }
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

    auto& operator*() const noexcept { return m_coroutine.promise(); }

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
    if(this != &other)
    {
      if(m_coroutine)
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
    if(m_coroutine)
    {
      m_coroutine.destroy();
    }
  }

  // Range-based for loop support.
  iterator begin() noexcept
  {
    if(m_coroutine)
    {
      m_coroutine.resume();
    }

    return iterator{m_coroutine};
  }

  std::default_sentinel_t end() const noexcept { return {}; }

private:
  handle m_coroutine;
};

template <typename Out>
class generator<Out, void>
{
public:
  // Types used by the coroutine
  struct promise_type
  {
    Out current_command;
    generator get_return_object() { return generator{handle::from_promise(*this)}; }

    static std::suspend_always initial_suspend() noexcept { return {}; }

    static std::suspend_always final_suspend() noexcept { return {}; }

    template <typename T>
    std::suspend_always yield_value(T&& value) noexcept
    {
      current_command = std::move(value);
      return std::suspend_always{};
    }

    void return_void() noexcept { }

    // Disallow co_await in generator coroutines.
    void await_transform() = delete;

    [[noreturn]] static void unhandled_exception() { std::abort(); }
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

    auto& operator*() const noexcept { return m_coroutine.promise(); }

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
    if(this != &other)
    {
      if(m_coroutine)
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
    if(m_coroutine)
    {
      m_coroutine.destroy();
    }
  }

  // Range-based for loop support.
  iterator begin() noexcept
  {
    if(m_coroutine)
    {
      m_coroutine.resume();
    }

    return iterator{m_coroutine};
  }

  std::default_sentinel_t end() const noexcept { return {}; }

private:
  handle m_coroutine;
};

struct suspend
{
  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<>) const noexcept { }
  void await_resume() const noexcept { }
};
//static const constexpr auto qSuspend = QSuspend{};

template <typename Promise>
class task
{
public:
  using promise_type = Promise;

  explicit task(std::coroutine_handle<Promise> handle) noexcept
      : m_handle{std::move(handle)}
  {
  }

  task(task&& other) noexcept
      : m_handle{std::exchange(other.m_handle, {})}
  {
  }

  task& operator=(task&& other) noexcept
  {
    m_handle = std::exchange(other.m_handle, {});
    return *this;
  }

  ~task()
  {
    if(m_handle)
      m_handle.destroy();
  }

  task(const task&) = delete;
  task& operator=(const task&) = delete;

  bool resume()
  {
    if(!m_handle.done())
      m_handle.resume();
    return !m_handle.done();
  }

private:
  std::coroutine_handle<Promise> m_handle;
};

}
