#pragma once
#include <ossia/network/value/value.hpp>
#include <avnd/introspection/widgets.hpp>

#include <QString>

#include <array>
#include <vector>
#include <utility>
#include <memory>
#include <string>
#include <string_view>

namespace oscr
{
template <std::size_t N>
auto to_combobox_range(const std::string_view (&val)[N])
{
  std::vector<std::pair<QString, ossia::value>> vec;
  for(int i = 0; i < N; i++)
    vec.emplace_back(val[i].data(), i);
  return vec;
}

template <std::size_t N>
auto to_combobox_range(const std::array<std::string_view, N>& val)
{
  std::vector<std::pair<QString, ossia::value>> vec;
  for(int i = 0; i < N; i++)
    vec.emplace_back(val[i].data(), i);
  return vec;
}

std::vector<std::pair<QString, ossia::value>> to_combobox_range(const auto& in)
{
  std::vector<std::pair<QString, ossia::value>> vec;
  for(auto& v : avnd::to_const_char_array(in))
    vec.emplace_back(v.first, v.second);
  return vec;
}
}
