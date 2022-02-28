#pragma once
#include <avnd/helpers/static_string.hpp>
#include <boost/container/small_vector.hpp>

#include <string_view>

namespace avnd
{

template <static_string lit>
struct midi_bus
{
  static consteval auto name() { return std::string_view{lit.value}; }

  struct message
  {
    boost::container::small_vector<uint8_t, 15> bytes;
    int64_t timestamp{};
  };

  boost::container::small_vector<message, 2> midi_messages;

  operator auto &() noexcept { return midi_messages; }
  operator const auto &() const noexcept { return midi_messages; }

  auto size() const noexcept { return midi_messages.size(); }
  auto empty() const noexcept { return midi_messages.size(); }

  auto begin() noexcept { return midi_messages.begin(); }
  auto end() noexcept { return midi_messages.end(); }
  auto cbegin() noexcept { return midi_messages.cbegin(); }
  auto cend() noexcept { return midi_messages.cend(); }
  auto begin() const noexcept { return midi_messages.begin(); }
  auto end() const noexcept { return midi_messages.end(); }
  auto cbegin() const noexcept { return midi_messages.cbegin(); }
  auto cend() const noexcept { return midi_messages.cend(); }

  auto& front() const noexcept { return midi_messages.front(); }
  auto& back() const noexcept { return midi_messages.back(); }
  auto& front() noexcept { return midi_messages.front(); }
  auto& back() noexcept { return midi_messages.back(); }

  auto& operator[](std::size_t i) const noexcept { return midi_messages[i]; }

  void push_back(const message& msg) { midi_messages.push_back(msg); }
  void push_back(message&& msg) { midi_messages.push_back(std::move(msg)); }
  template <typename... Args>
  void emplace_back(Args&&... t)
  {
    midi_messages.emplace_back(std::forward<Args>(t)...);
  }
};
}
