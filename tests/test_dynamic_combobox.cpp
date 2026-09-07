// The two runtime-populated combobox ports are additive: a binding that has
// never heard of them must still see plain comboboxes, and a binding that has
// must be able to tell the two apart. Both are compile-time properties, so
// they are checked here rather than at run time.

#include <avnd/concepts/dynamic_items.hpp>
#include <avnd/concepts/folder_items.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/range.hpp>
#include <halp/controls.enums.hpp>
#include <halp/dynamic_combobox.hpp>
#include <halp/folder_combobox.hpp>

#include <string>
#include <type_traits>
#include <vector>

namespace
{
using dyn = halp::dynamic_combobox<"File">;
using folder = halp::folder_combobox<"File", "Folder", "wav aif">;

enum class Choice
{
  a,
  b
};
using plain = halp::combobox_t<"Plain", Choice>;

// 1. Both look like ordinary comboboxes to every existing binding, which is
//    what makes the addition backwards compatible.
static_assert(avnd::enum_ish_parameter<dyn>);
static_assert(avnd::enum_ish_parameter<folder>);
static_assert(avnd::enum_ish_parameter<plain>);

// 2. A binding that knows the concepts can pick each one out...
static_assert(avnd::dynamic_items_parameter<dyn>);
static_assert(avnd::folder_items_parameter<folder>);

// 3. ... without confusing them with each other, or with a plain combobox.
static_assert(!avnd::folder_items_parameter<dyn>);
static_assert(!avnd::dynamic_items_parameter<folder>);
static_assert(!avnd::dynamic_items_parameter<plain>);
static_assert(!avnd::folder_items_parameter<plain>);

// 4. The value each port carries is the one its host is expected to write:
//    an index for the dynamic combobox, a file name for the folder one.
static_assert(std::is_same_v<decltype(dyn::value), int>);
static_assert(std::is_same_v<decltype(folder::value), std::string>);

// 5. update_items takes the item list by value, so a processor may hand over a
//    vector built on any thread.
static_assert(std::is_invocable_v<decltype(dyn::update_items), std::vector<std::string>>);

// 6. The folder port advertises which sibling to list and what to keep.
static_assert(folder::folder_port() == std::string_view{"Folder"});
static_assert(folder::extensions() == std::string_view{"wav aif"});

// 7. Both keep a usable static fallback, so a binding that never fills
//    update_items still renders a combobox with one item rather than none.
static_assert(avnd::get_range<dyn>().values.size() == 1);
static_assert(avnd::get_range<folder>().values.size() == 1);

// 8. The defaults name a folder port and accept every extension.
using folder_default = halp::folder_combobox<"F">;
static_assert(folder_default::folder_port() == std::string_view{"Folder"});
static_assert(folder_default::extensions() == std::string_view{""});
}

int main()
{
  // Assigning through the port's conversion operators is how objects use them.
  dyn d;
  d = 3;
  folder f;
  f = std::string{"kick.wav"};
  return (d.value == 3 && f.value == "kick.wav") ? 0 : 1;
}
