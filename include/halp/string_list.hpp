#pragma once
#include <halp/dynamic_port.hpp>
#include <halp/static_string.hpp>

#include <string>
#include <utility>
#include <vector>

namespace halp
{
// The identity travels with the text through edits, moves, presets and undo.
// Hosts serialize this as a list of [integer identity, string] pairs.
using string_list_value = std::vector<std::pair<int, std::string>>;

template <static_string Name>
struct string_list
{
  enum class widget
  {
    string_list
  };
  static constexpr auto name() { return std::string_view{Name.value}; }
  string_list_value value;
};

// A keyed resize preserves each surviving port, its value and its cables.
// Keys must be unique and >= 10000 (below that is reserved for static ports).
// Editing a label keeps its key; moving a row moves its key; deleting a row
// removes only that port. Hosts without keyed support can use the count hook.
template <typename Port>
struct keyed_dynamic_port : dynamic_port<Port>
{
  std::function<void(const string_list_value&)> request_port_rows;

  void set_rows(const string_list_value& rows)
  {
    if(request_port_rows)
      request_port_rows(rows);
    else if(this->request_port_resize)
      this->request_port_resize(rows.size());
  }
};
}
