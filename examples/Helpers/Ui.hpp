#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/processor.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/layout.hpp>
#include <avnd/helpers/meta.hpp>
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
    avnd::dynamic_audio_bus<"In", double> audio;
    avnd::hslider_i32<"Int"> int_ctl;
    avnd::knob_f32<"Float", avnd::range{.min = -1000., .max = 1000., .init = 100.}> float_ctl;

    avnd::toggle<"T1", avnd::toggle_setup{.init = true}> t1;
    avnd::toggle<"T2", avnd::toggle_setup{.init = false}> t2;
    avnd::maintained_button<"B1"> b1;
    avnd::impulse_button<"B2"> b2;
    avnd__enum("Simple Enum", Peg, Square, Peg, Round, Hole) e1;

  } inputs;

  struct outs
  {
    avnd::dynamic_audio_bus<"Out", double> audio;
    avnd::hbargraph_f32<"Meter", avnd::range{.min = -1., .max = 1., .init = 0.}> measure;
  } outputs;

  double phase = 0;
  void operator()(int N)
  {
      outputs.measure = std::sin(phase);
      phase += N * 0.001;
  }

  struct ui_layout {
      // If your compiler is recent enough:
      // using enum avnd::colors;
      // using enum avnd::layouts;

      avnd_meta(name, "Main")
      avnd_meta(layout, avnd::layouts::hbox)
      avnd_meta(background, avnd::colors::mid)

      struct {
        avnd_meta(name, "Widget")
        avnd_meta(layout, avnd::layouts::vbox)
        avnd_meta(background, avnd::colors::dark)

        avnd::item<&ins::int_ctl> widget;
        avnd::item<&outs::measure> widget2;
      } widgets;

      avnd::spacing spc{.width = 20, .height = 20};

      struct {
        avnd_meta(name, "Group")
        avnd_meta(layout, avnd::layouts::group)
        avnd_meta(background, avnd::colors::light)
        struct {
          avnd_meta(layout, avnd::layouts::hbox)
          avnd::label l1{.text = "label 1"};
          avnd::spacing spacing{.width = 20, .height = 20};
          avnd::label l2{.text = "label 2"};
        } a_hbox;
      } b_group;

      struct {
        avnd_meta(name, "Tabs")
        avnd_meta(layout, avnd::layouts::tabs)
        avnd_meta(background, avnd::colors::darker)

        struct {
          avnd_meta(layout, avnd::layouts::hbox)
          avnd_meta(name, "HBox")
          avnd::label l1{.text = "label 1"};
          avnd::label l2{.text = "label 2"};
        } a_hbox;

        struct {
          avnd_meta(layout, avnd::layouts::vbox)
          avnd_meta(name, "VBox")
          avnd::label l1{.text = "label 1"};
          avnd::label l2{.text = "label 2"};
        } a_vbox;
      } a_tabs;

      struct {
        avnd_meta(layout, avnd::layouts::split)
        avnd_meta(name, "split")
        avnd_meta(width, 400)
        avnd_meta(height, 200)
        struct {
            avnd_meta(layout, avnd::layouts::vbox)
            avnd::label l1{.text = "some long foo"};
            avnd::item<&ins::t1> a;
            avnd::item<&ins::t2> b;
        } a_widg;
        struct {
            avnd_meta(layout, avnd::layouts::vbox)
            avnd::label l2{.text = "other bar"};
            avnd::item<&ins::e1> c;

            struct {
                avnd_meta(layout, avnd::layouts::hbox)
                avnd::item<&ins::b1> a;
                avnd::item<&ins::b2> b;
            } c2;
        } b_widg;
      } a_split;

      struct {
        avnd_meta(name, "Grid")
        avnd_meta(layout, avnd::layouts::grid)
        avnd_meta(background, avnd::colors::lighter)
        avnd_meta(columns, 3)
        avnd_meta(padding, 5)

        avnd::item<&ins::float_ctl> widget{{.scale = 0.8}};

        avnd::label l1{.text = "A"};
        avnd::label l2{.text = "B"};
        avnd::label l3{.text = "C"};
        avnd::label l4{.text = "D"};
        avnd::label l5{.text = "E"};
      } a_grid;
  };
};
}
