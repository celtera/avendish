#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/concepts/processor.hpp>
#include <avnd/concepts/painter.hpp>
#include <avnd/wrappers/controls.hpp>
#include <halp/custom_widgets.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <cmath>

#include <cstdio>

//Modif
#include <iostream>
#include <string.h>
#include <math.h>
#include <stdio.h>

using namespace std;

namespace Spat
{
struct MyMosca
{
    static consteval auto name() { return "MoscaUi example"; }
    static consteval auto c_name() { return "mosca_ui"; }
    static consteval auto uuid() { return "f253d48c-b090-497c-a561-ec442efdcc92"; }
    //static consteval auto category() { return "Test"; }

    struct custom_mosca
    {
        static constexpr double width() { return 300.; } // X
        static constexpr double height() { return 300.; } // Y

        void set_value(const auto& control, halp::xy_type<float> value)
        {
          this->value = avnd::map_control_to_01(control, value);
        }

        static auto value_to_control(auto& control, halp::xy_type<float> value)
        {
          return avnd::map_control_from_01(control, value);
        }

        void paint(avnd::painter auto ctx)
        {
            double c_x = width()/2;
            double c_y = height()/2;
            double c_r = 150;
            double c_r_bis = 4;

            float m_x = value.x * width();
            float m_y = value.y * height();
            float m_r = 15.;

            ctx.set_stroke_color({200, 200, 200, 255});
            ctx.set_stroke_width(2.);
            ctx.set_fill_color({120, 120, 120, 255});
            ctx.begin_path();
            ctx.draw_rect(0., 0., width(), height());
            ctx.fill();
            ctx.stroke();

            ctx.begin_path();
            ctx.set_stroke_color({255, 255, 255, 255});
            ctx.set_fill_color({255, 255, 255, 255});
            ctx.draw_circle(c_x, c_y, c_r);
            ctx.fill();
            ctx.stroke();

            ctx.begin_path();
            ctx.set_stroke_color({255, 255, 255, 255});
            ctx.set_fill_color({0, 0, 0, 255});
            ctx.draw_circle(c_x, c_y, c_r_bis);
            ctx.fill();
            ctx.stroke();

            float formula = sqrt(pow((m_x - c_x), 2) + pow((m_y - c_y), 2));
            if(formula + (m_r - 5) < c_r){
                ctx.begin_path();
                ctx.set_fill_color({90, 90, 90, 255});
                ctx.draw_circle(m_x, m_y, m_r);
                ctx.fill();
                ctx.close_path();

                ctx.begin_path();
                ctx.set_fill_color({255, 255, 255, 255});
                ctx.set_font("Ubuntu");
                ctx.set_font_size(15);
                ctx.draw_text(m_x-6, m_y+7, "1");
                ctx.fill();
            }

            /*
            ctx.begin_path();
            ctx.set_fill_color({255, 255, 255, 255});
            ctx.set_font("Ubuntu");
            ctx.set_font_size(15);
            std::stringstream strs;
            strs << value.x;
            std::string test = strs.str();
            ctx.draw_text(50,100, test);
            std::stringstream strs2;
            strs2 << value.y;
            std::string test2 = strs2.str();
            ctx.draw_text(50,200, test2);

            ctx.fill();
            */

        }

        bool mouse_press(double x, double y)
        {
            transaction.start();
            mouse_move(x, y);
            return true;
        }

        void mouse_move(double x, double y)
        {
            halp::xy_type<float> res;
            res.x = std::clamp(x / width(), 0., 1.);

            res.y = std::clamp(y / height(), 0., 1.);
            transaction.update(res);
        }

        void mouse_release(double x, double y)
        {
            mouse_move(x, y);
            transaction.commit();
        }

        halp::transaction<halp::xy_type<float>> transaction;
        halp::xy_type<float> value{};
    };

    struct ins
    {
        halp::dynamic_audio_bus<"Input", double> audio;

        halp::hslider_f32<"A", halp::range{.min = -10, .max = 10, .init = 0}> a;
        halp::hslider_f32<"B", halp::range{.min = -10, .max = 10, .init = 0}> b;
        halp::spinbox_i32<"Int spinbox", halp::range{0, 1000, 10}> c;
        halp::hslider_f32<"A", halp::range{.min = -10, .max = 10, .init = 0}> d;
        halp::hslider_f32<"B", halp::range{.min = -10, .max = 10, .init = 0}> e;
    } inputs;

    struct outs
    {
        halp::dynamic_audio_bus<"Output", double> audio;
    } outputs;

    struct ui {
        halp_meta(name, "Main")
        halp_meta(layout, halp::layouts::hbox)
        halp_meta(background, "background.svg")
        halp_meta(width, 600)
        halp_meta(height, 300)
        halp_meta(font, "Inconsolata")

        struct {
            halp_meta(name, "Mosca")
            halp_meta(layout, halp::layouts::vbox)
            halp_meta(background, halp::colors::mid)

            halp::custom_item<custom_mosca, &ins::a> widget{{.x = 500, .y = 920}};
        } mosca;

        struct {
            halp_meta(name, "Panel")
            halp_meta(layout, halp::layouts::vbox)
            halp_meta(width, 300)
            halp_meta(height, 300)

            halp::item<&ins::a> w1;
            halp::item<&ins::b> w2;
            halp::item<&ins::c> w3;
            halp::item<&ins::d> w4;
            halp::item<&ins::e> w5;
        } panel;

    };

    void operator()(halp::tick t, halp::xy_type<float> value)
    {
      // Process the input buffer
      for (int i = 0; i < inputs.audio.channels; i++)
      {
        auto* in = inputs.audio[i];
        auto* out = outputs.audio[i];

        float& prev = this->previous_values[i];

        for (int j = 0; j < t.frames; j++)
        {
          out[j] = value.x * in[j];
          prev = out[j];
        }
      }
    }
private:
  // Here we have some state which depends on the host configuration (number of channels, etc).
  std::vector<float> previous_values{};
};
}
