#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/processor.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <cmath>

#include <cstdio>

namespace examples::helpers
{
/**
 * Example to test UI.
 */
struct Ui
{
  static consteval auto name() { return "UI example"; }
  static consteval auto c_name() { return "avnd_ui"; }
  static consteval auto uuid() { return "794ab8b2-94cf-4be4-8c8c-b7c6aee3e6da"; }

  struct ins
  {
    halp::dynamic_audio_bus<"In", double> audio;
    halp::hslider_i32<"Int"> int_ctl;
    halp::knob_f32<"Float", halp::range{.min = -1000., .max = 1000., .init = 100.}> float_ctl;

    halp::toggle<"T1", halp::toggle_setup{.init = true}> t1;
    halp::toggle<"T2", halp::toggle_setup{.init = false}> t2;
    halp::maintained_button<"B1"> b1;
    halp::impulse_button<"B2"> b2;
    halp__enum("Simple Enum", Peg, Square, Peg, Round, Hole) e1;

  } inputs;

  struct outs
  {
    halp::dynamic_audio_bus<"Out", double> audio;
    halp::hbargraph_f32<"Meter", halp::range{.min = -1., .max = 1., .init = 0.}> measure;
  } outputs;

  double phase = 0;
  void operator()(int N)
  {
      outputs.measure = std::sin(phase);
      phase += N * 0.001;
  }

  struct ui {
      // If your compiler is recent enough:
      // using enum halp::colors;
      // using enum halp::layouts;

      halp_meta(name, "Main")
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::mid)

      struct {
        halp_meta(name, "Widget")
        halp_meta(layout, halp::layouts::vbox)
        halp_meta(background, halp::colors::dark)

        halp::item<&ins::int_ctl> widget;
        halp::item<&outs::measure> widget2;
      } widgets;

      halp::spacing spc{.width = 20, .height = 20};

      struct {
        halp_meta(name, "Group")
        halp_meta(layout, halp::layouts::group)
        halp_meta(background, halp::colors::light)
        struct {
          halp_meta(layout, halp::layouts::hbox)
          halp::label l1{.text = "label 1"};
          halp::spacing spacing{.width = 20, .height = 20};
          halp::label l2{.text = "label 2"};
        } a_hbox;
      } b_group;

      struct {
        halp_meta(name, "Tabs")
        halp_meta(layout, halp::layouts::tabs)
        halp_meta(background, halp::colors::darker)

        struct {
          halp_meta(layout, halp::layouts::hbox)
          halp_meta(name, "HBox")
          halp::label l1{.text = "label 1"};
          halp::label l2{.text = "label 2"};
        } a_hbox;

        struct {
          halp_meta(layout, halp::layouts::vbox)
          halp_meta(name, "VBox")
          halp::label l1{.text = "label 1"};
          halp::label l2{.text = "label 2"};
        } a_vbox;
      } a_tabs;

      struct {
        halp_meta(layout, halp::layouts::split)
        halp_meta(name, "split")
        halp_meta(width, 300)
        halp_meta(height, 200)
        struct {
            halp_meta(layout, halp::layouts::vbox)
            halp::label l1{.text = "some long foo"};
            halp::item<&ins::t1> a;
            halp::item<&ins::t2> b;
        } a_widg;
        struct {
            halp_meta(layout, halp::layouts::vbox)
            halp::label l2{.text = "other bar"};
            halp::item<&ins::e1> c;

            struct {
                halp_meta(layout, halp::layouts::hbox)
                halp::item<&ins::b1> a;
                halp::item<&ins::b2> b;
            } c2;
        } b_widg;
      } a_split;

      struct {
        halp_meta(name, "Grid")
        halp_meta(layout, halp::layouts::grid)
        halp_meta(background, halp::colors::lighter)
        halp_meta(columns, 3)
        halp_meta(padding, 5)

        halp::item<&ins::float_ctl> widget{};

        halp::label l1{.text = "A"};
        halp::label l2{.text = "B"};
        halp::label l3{.text = "C"};
        halp::label l4{.text = "D"};
        halp::label l5{.text = "E"};
      } a_grid;
  };
};
}
