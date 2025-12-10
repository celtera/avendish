#pragma once
#include <boost/container/vector.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace halp
{

struct raw_buffer
{
  unsigned char* raw_data{};
  int64_t byte_size{};
  int64_t byte_offset{};
  bool changed{};

  operator std::span<unsigned char>() const noexcept
  {
    return {raw_data + byte_offset, std::size_t(byte_size - byte_offset)};
  }
};

struct raw_output_buffer : raw_buffer
{
  std::function<void(const char* data, int64_t offset, int64_t bytesize)> upload;
};

template <typename T>
struct typed_buffer
{
  T* elements{};
  int64_t element_count{};
  bool changed{};

  operator std::span<T>() const noexcept
  {
    return {elements, std::size_t(element_count)};
  }
};

struct gpu_buffer
{
  void* handle{};
  int byte_size{};
  int byte_offset{};
  bool changed{};
};
template <static_string lit>
struct cpu_buffer_input
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  operator const halp::raw_buffer&() const noexcept { return buffer; }
  operator halp::raw_buffer&() noexcept { return buffer; }

  template <typename T>
  auto cast()
  {
    return std::span<T>{
        reinterpret_cast<T*>(buffer.raw_data), buffer.byte_size / sizeof(T)};
  }
  template <typename T>
  auto cast() const noexcept
  {
    return std::span<const T>{
        reinterpret_cast<const T*>(buffer.raw_data), buffer.byte_size / sizeof(T)};
  }

  halp::raw_buffer buffer{};
};

template <static_string lit>
struct cpu_buffer_output
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  halp::raw_output_buffer buffer{};

  template <typename T>
  auto create(int64_t sz)
  {
    storage.resize(sz * sizeof(T), boost::container::default_init);
    buffer.changed = false;
    buffer.raw_data = storage.data();
    buffer.byte_size = storage.size();
    return std::span<T>{reinterpret_cast<T*>(storage.data()), std::size_t(sz)};
  }

  template <typename T>
  auto cast()
  {
    return std::span<T>{
        reinterpret_cast<T*>(storage.data()), storage.size() / sizeof(T)};
  }

  void upload() noexcept { buffer.changed = true; }

  using uninitialized_bytes = boost::container::vector<unsigned char>;
  uninitialized_bytes storage;
};

template <static_string lit, typename Type>
struct typed_cpu_buffer_input
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  halp::typed_buffer<Type> buffer{};

  operator const halp::typed_buffer<Type>&() const noexcept { return buffer; }
  operator halp::typed_buffer<Type>&() noexcept { return buffer; }
  operator std::span<Type>() const noexcept { return buffer; }
};

template <static_string lit, typename Type>
struct typed_cpu_buffer_output
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  halp::typed_buffer<Type> buffer{};

  auto create(int64_t sz)
  {
    storage.resize(sz, boost::container::default_init);
    buffer.changed = false;
    buffer.elements = storage.data();
    buffer.element_count = sz;
    return std::span<Type>{storage};
  }

  operator std::span<Type>() const noexcept { return buffer; }

  void upload() noexcept { buffer.changed = true; }

  using uninitialized_data = boost::container::vector<Type>;
  uninitialized_data storage;
};

template <static_string lit>
struct gpu_buffer_input
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  operator const halp::gpu_buffer&() const noexcept { return buffer; }
  operator halp::gpu_buffer&() noexcept { return buffer; }

  halp::gpu_buffer buffer{};
};

template <static_string lit>
struct gpu_buffer_output
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  operator const halp::gpu_buffer&() const noexcept { return buffer; }
  operator halp::gpu_buffer&() noexcept { return buffer; }

  halp::gpu_buffer buffer{};
};

}
