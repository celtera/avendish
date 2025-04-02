#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/meta.hpp>
#include <halp/modules.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <functional>
#include <string_view>
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
  multi_control
};
enum class colors
{
  darker,
  dark,
  mid,
  light,
  lighter
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

// When clang supports P2082R1, we could just use a deduction guide instead...

template <auto F>
struct item : item_base
{
  decltype(F) model = F;
};

template <auto F>
struct control : item_base
{
  decltype(F) model = F;
  using control_member_type = halp::member_type_t<decltype(F)>;
  using control_value_type = decltype(control_member_type::value);
  control_value_type value;
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
