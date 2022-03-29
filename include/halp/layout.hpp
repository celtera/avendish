#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>
#include <halp/meta.hpp>

#include <string_view>
#include <functional>

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
  widget
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
  avnd_meta(layout, layouts::spacing)
  int width{}, height{};
};
struct label
{
  avnd_meta(layout, layouts::widget)
  std::string_view text;
};

struct item_base
{
  avnd_meta(layout, layouts::control)
  double x = 0.0;
  double y = 0.0;
  double scale = 1.0;
};

// When clang supports P2082R1, we could just use a deduction guide instead...
template <auto F>
struct item : item_base
{
  decltype(F) control = F;
};

template <typename M, typename L, typename T>
struct prop
{
  std::function<T(M& self, L& layout)> get;
  std::function<void(M& self, L& layout, const T&)> set;
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
