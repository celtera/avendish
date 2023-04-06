#pragma once
#include <cstdint>

namespace avnd
{
template <typename T, std::size_t N>
struct ebo_array
{
  T arr[N];
  constexpr void fill(const auto& val) noexcept
  {
    for(int i = 0; i < N; i++)
      arr[i] = val;
  }
  constexpr T& operator[](std::size_t i) noexcept { return arr[i]; }
  constexpr const T& operator[](std::size_t i) const noexcept { return arr[i]; }
};
template <typename T>
struct ebo_array<T, 0>
{
  constexpr void fill(const auto& val) noexcept { }
  constexpr T operator[](std::size_t i) const noexcept
  {
    std::terminate();
    return {};
  }
};

template <typename T, std::size_t N>
constexpr const T* begin(const ebo_array<T, N>& e) noexcept
{
  return e.arr;
}
template <typename T, std::size_t N>
constexpr const T* end(const ebo_array<T, N>& e) noexcept
{
  return e.arr + N;
}
template <typename T, std::size_t N>
constexpr T* begin(ebo_array<T, N>& e) noexcept
{
  return e.arr;
}
template <typename T, std::size_t N>
constexpr T* end(ebo_array<T, N>& e) noexcept
{
  return e.arr + N;
}
template <typename T, std::size_t N>
constexpr const T* cbegin(const ebo_array<T, N>& e) noexcept
{
  return e.arr;
}
template <typename T, std::size_t N>
constexpr const T* cend(const ebo_array<T, N>& e) noexcept
{
  return e.arr + N;
}
template <typename T, std::size_t N>
constexpr const T* cbegin(ebo_array<T, N>& e) noexcept
{
  return e.arr;
}
template <typename T, std::size_t N>
constexpr const T* cend(ebo_array<T, N>& e) noexcept
{
  return e.arr + N;
}
}
