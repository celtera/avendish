#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/meta.hpp>
#include <halp/modules.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <array>
#include <functional>
#include <string_view>
#include <tuple>
HALP_MODULE_EXPORT
namespace halp
{

enum class layouts
{
  container,
  hbox,
  vbox,
  grid,
  split,
  tabs,
  group,
  spacing,
  control,
  widget,
  custom,
  custom_control,
  multi_control,
  // Appended: existing values unchanged
  // Titled, padded vbox
  section,
  // Named rows under shared column titles, from a static columns().
  // Controls hide their names unless their look sets one.
  table,
  // Tabs selected from a strip of cells, showing each page's `summary` member
  strip_detail
};

enum class colors
{
  darker,
  dark,
  mid,
  light,
  lighter,

  background_darker,
  background_dark,
  background_mid,
  background_light,
  background_lighter,

  runtime_value_light,
  runtime_value_mid,
  runtime_value_dark,

  editable_value_light,
  editable_value_mid,
  editable_value_dark,
};

struct spacing
{
  halp_meta(layout, layouts::spacing)
  int width{}, height{};
};

struct label
{
  halp_meta(layout, layouts::widget)
  std::string_view text;
};

struct item_base
{
  halp_meta(layout, layouts::control)
  double x = 0.0;
  double y = 0.0;
  double scale = 1.0;
};

// Not "small": a <windows.h> macro
enum class control_size
{
  normal,
  compact,
  large
};

enum class value_display
{
  always,
  hover // while hovered or dragged
};

// Widget override, where the host supports it
enum class control_widget
{
  automatic,
  knob, // for a slider parameter
  combo // for an enumeration
};

// Presentation of a control in a layout. Unsupported options are ignored.
//
//   halp::item<&ins::ratio_0, halp::look{.label = "Ratio"}> ratio;
struct look
{
  // Display only: the parameter keeps its name. Empty: unchanged.
  char label[64]{};
  bool hide_label{false};
  control_size size{control_size::normal};
  value_display value{value_display::always};
  control_widget widget{control_widget::automatic};
};

//
//   halp::item<&ins::ratio_0, halp::label_as("Ratio")> ratio;
consteval look label_as(std::string_view name)
{
  look l;
  for(std::size_t i = 0; i < name.size() && i + 1 < sizeof(l.label); ++i)
    l.label[i] = name[i];
  return l;
}

// Controls greyed out unless F holds one of Values. Layout is unchanged.
//
//   struct : halp::enabled_when<&ins::engine, engine::plate, engine::cymbal>
//   {
//     halp_meta(layout, halp::layouts::hbox)
//     halp::item<&ins::cascade> cascade;
//   } cascade;
template <auto F, auto... Values>
struct enabled_when
{
  static constexpr auto condition_control = F;
  static constexpr auto condition_values() { return std::array{Values...}; }
  static constexpr bool condition_hides = false;
};

// Hidden instead of greyed out; the space is kept
template <auto F, auto... Values>
struct visible_when : enabled_when<F, Values...>
{
  static constexpr bool condition_hides = true;
};

// When clang supports P2082R1, we could just use a deduction guide instead...

template <auto F, look L = look{}>
struct item : item_base
{
  static constexpr look presentation = L;
  decltype(F) model = F;
};

enum class display_style
{
  text, // value, or entry name
  bar,  // position in the range
  title // as the enclosing strip cell's title, while not empty
};

// Read-only view of a parameter. Hosts without one show the control.
template <auto F, display_style S = display_style::text>
struct display : item_base
{
  static constexpr bool display_only = true;
  static constexpr display_style style = S;
  decltype(F) model = F;
};

template <auto F, look L = look{}>
struct control : item_base
{
  static constexpr look presentation = L;
  decltype(F) model = F;
  using control_member_type = halp::member_type_t<decltype(F)>;
  using control_value_type = decltype(control_member_type::value);
  control_value_type value{};
};

struct image_item_base
{
  halp_meta(layout, layouts::control)
  double x = 0.0;
  double y = 0.0;
  double scale = 1.0;
  std::string_view image;
};

template <auto F>
struct image_item : image_item_base
{
  decltype(F) model = F;
};

template <typename T>
struct custom_item_base
{
  halp_meta(layout, layouts::custom)
  using item_type = T;

  double x = 0.0;
  double y = 0.0;
  double scale = 1.0;
};

template <typename T, auto F>
struct custom_item : T
{
  halp_meta(layout, layouts::custom)
  using item_type = T;

  double x = 0.0;
  double y = 0.0;
  double scale = 1.0;

  decltype(F) model = F;
};

template <typename T>
struct custom_actions_item : T
{
  halp_meta(layout, halp::layouts::custom)

  double x = 0.0;
  double y = 0.0;
  double scale = 1.0;
};

template <typename T, auto F>
struct custom_control : T
{
  halp_meta(layout, halp::layouts::custom_control)

  double x = 0.0;
  double y = 0.0;
  double scale = 1.0;

  decltype(F) model = F;
};


template <typename T, auto F>
struct multi_control : T
{
  halp_meta(layout, halp::layouts::multi_control)

  double x = 0.0;
  double y = 0.0;
  double scale = 1.0;

  decltype(F) model = F;
};

// Custom widget editing several parameters. T: as custom_control, plus:
//   std::array<double, sizeof...(F)> values;  // normalized, kept up to date
//   halp::multi_transaction transaction;       // to edit them
template <typename T, auto... F>
struct custom_multi_control : T
{
  halp_meta(layout, halp::layouts::multi_control)
  static constexpr auto models() { return std::tuple{F...}; }

  double x = 0.0;
  double y = 0.0;
  double scale = 1.0;
};

template <typename M, typename L, typename T>
struct prop
{
  std::function<T(M& self, L& layout)> get;
  std::function<void(M& self, L& layout, const T&)> set;
};

template <auto F, typename UI = void>
struct recursive_group_item;

template <auto F, typename UI>
struct recursive_group_item
{
  decltype(F) group = F;
  using group_ui = UI;
  group_ui ui;
};

// template<typename T>
// using prop = ::prop<Ui, layout, T>;

/* first tentative, not very good but still there for posterity...
template <int w>
struct hspace
{
  enum
  {
    spacing
  };
  static constexpr auto width() { return w; }
  static constexpr auto height() { return 1; }
};
template <int h>
struct vspace
{
  enum
  {
    spacing
  };
  static constexpr auto width() { return 1; }
  static constexpr auto height() { return h; }
};
template <int w, int h>
struct space
{
  enum
  {
    spacing
  };
  static constexpr auto width() { return w; }
  static constexpr auto height() { return h; }
};
//
// struct hbox {
//     enum { hbox };
// };
// struct vbox {
//     enum { vbox };
// };
//
// template <static_string lit>
// struct group {
//     enum { group };
//     static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
// };
// struct tabs {
//     enum { tabs };
// };
// template<int w, int h>
// struct split {
//     enum { split };
//     static constexpr auto width() { return w; }
//     static constexpr auto height() { return h; }
// };
// template<static_string lit, typename Layout>
// struct tab : Layout {
//     static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
// };

#define avnd_cat2(a, b) a##b
#define avnd_cat(a, b) avnd_cat2(a, b)
#define avnd_lay struct:

#define avnd_hbox \
  struct          \
  {               \
    enum          \
    {             \
      hbox        \
    };
#define avnd_vbox \
  struct          \
  {               \
    enum          \
    {             \
      vbox        \
    };
#define avnd_tabs \
  struct          \
  {               \
    enum          \
    {             \
      tabs        \
    };
#define avnd_split(w, h)                        \
  struct                                        \
  {                                             \
    enum                                        \
    {                                           \
      split                                     \
    };                                          \
    static constexpr auto width() { return w; } \
    static constexpr auto height() { return h; }

#define avnd_group(Name) \
  struct                 \
  {                      \
    enum                 \
    {                    \
      group              \
    };                   \
    static clang_buggy_consteval auto name() { return std::string_view{Name}; }
#define avnd_tab(Name, Type) \
  struct                     \
  {                          \
    enum                     \
    {                        \
      Type                   \
    };                       \
    static clang_buggy_consteval auto name() { return std::string_view{Name}; }

#define avnd_close \
  }                \
  avnd_cat(widget_, __LINE__)

#define avnd_widget(Inputs, Ctl) decltype(&Inputs::Ctl) Ctl = &Inputs::Ctl;

*/
}
