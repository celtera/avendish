#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/processor.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/layout.hpp>
#include <avnd/helpers/meta.hpp>
#include <cmath>

#include <cstdio>

namespace avnd
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
  avnd_meta(layout, layouts::spacing) int width{}, height{};
};
struct label
{
  avnd_meta(layout, layouts::widget) std::string_view text;
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
}
template <typename M, typename L, typename T>
struct prop
{
  std::function<T(M& self, L& layout)> get;
  std::function<void(M& self, L& layout, const T&)> set;
};
// template<typename T>
// using prop = ::prop<Ui, layout, T>;

namespace examples::helpers
{
/**
 * Example to test UI.
 */
struct Ui
{
  static consteval auto name() { return "UI example"; }
  static consteval auto c_name() { return "avnd_ui"; }
  static consteval auto uuid() { return "e1f0f202-6732-4d2d-8ee9-5957a51ae667"; }

  struct ins
  {
    avnd::dynamic_audio_bus<"In", double> audio;
    avnd::hslider_i32<"Int"> int_ctl;
    avnd::knob_f32<"Float", avnd::range{.min = -1000, .max = 1000, .init = 100}>
        float_ctl;

    avnd::toggle<"T1", avnd::toggle_setup{.init = true}> t1;
    avnd::toggle<"T2", avnd::toggle_setup{.init = false}> t2;
    avnd::maintained_button<"B1"> b1;
    avnd::impulse_button<"B2"> b2;
    avnd__enum("Simple Enum", Peg, Square, Peg, Round, Hole) e1;

  } inputs;

  struct outs
  {
    avnd::dynamic_audio_bus<"Out", double> audio;
  } outputs;

  void operator()(int N) { }

  struct ui_layout
  {
    using enum avnd::colors;
    using enum avnd::layouts;

    avnd_meta(name, "Main") avnd_meta(layout, hbox) avnd_meta(background, mid) struct
    {
      avnd_meta(name, "Widget") avnd_meta(layout, container) avnd_meta(background, dark)

          avnd::item<&ins::int_ctl> w1{{.x = 15, .y = 15}};
      avnd::item<&ins::float_ctl> w2{{.x = 120, .y = 30, .scale = 2.5}};

    } widgets;
  };
  /*
  struct ui_layout {
      using enum avnd::colors;
      using enum avnd::layouts;

      avnd_meta(name, "Main")
      avnd_meta(layout, hbox)
      avnd_meta(background, mid)
      struct {
        avnd_meta(name, "Widget")
        avnd_meta(layout, hbox)
        avnd_meta(background, dark)

        avnd::item<&ins::int_ctl> widget;
      } widgets;

      avnd::spacing spacing{.width = 20, .height = 20};

      struct {
        avnd_meta(name, "Group")
        avnd_meta(layout, group)
        avnd_meta(background, light)
        struct {
          avnd_meta(layout, hbox)
          avnd::label l1{.text = "label 1"};
          avnd::spacing spacing{.width = 20, .height = 20};
          avnd::label l2{.text = "label 2"};
        } a_hbox;
      } b_group;

      struct {
        avnd_meta(name, "Tabs")
        avnd_meta(layout, tabs)
        avnd_meta(background, darker)

        struct {
          avnd_meta(layout, hbox)
          avnd_meta(name, "HBox")
          avnd::label l1{.text = "label 1"};
          avnd::label l2{.text = "label 2"};
        } a_hbox;

        struct {
          avnd_meta(layout, vbox)
          avnd_meta(name, "VBox")
          avnd::label l1{.text = "label 1"};
          avnd::label l2{.text = "label 2"};
        } a_vbox;
      } a_tabs;

      struct {
        avnd_meta(layout, split)
        avnd_meta(name, "split")
        avnd_meta(width, 400)
        avnd_meta(height, 200)
        struct {
            avnd_meta(layout, vbox)
            avnd::label l1{.text = "some long foo"};
            avnd::item<&ins::t1> a;
            avnd::item<&ins::t2> b;
        } a_widg;
        struct {
            avnd_meta(layout, vbox)
            avnd::label l2{.text = "other bar"};
            avnd::item<&ins::e1> c;

            struct {
                avnd_meta(layout, hbox)
                avnd::item<&ins::b1> a;
                avnd::item<&ins::b2> b;
            } c2;
        } b_widg;
      } a_split;

      struct {
        avnd_meta(name, "Grid")
        avnd_meta(layout, grid)
        avnd_meta(background, lighter)
        avnd_meta(columns, 3)
        avnd_meta(padding, 5)

        avnd::item<&ins::float_ctl> widget{.scale = 0.8};

        avnd::label l1{.text = "A"};
        avnd::label l2{.text = "B"};
        avnd::label l3{.text = "C"};
        avnd::label l4{.text = "D"};
        avnd::label l5{.text = "E"};
      } a_grid;
  };
  */
};
}
