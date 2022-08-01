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

    using setup = halp::setup;
    using tick = halp::tick;

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

    struct custom_button
    {
        static constexpr double width() { return 100.; }
        static constexpr double height() { return 100.; }

        void paint(avnd::painter auto ctx)
        {
          ctx.set_stroke_color({200, 200, 200, 255});
          ctx.set_stroke_width(2.);
          ctx.set_fill_color({100, 100, 100, 255});
          ctx.begin_path();
          ctx.draw_rounded_rect(0., 0., width(), height(), 5);
          ctx.fill();
          ctx.stroke();

          ctx.set_fill_color({0, 0, 0, 255});
          ctx.begin_path();
          ctx.draw_text(20., 20., fmt::format("{}", press_count));
          ctx.fill();
        }

        bool mouse_press(double x, double y)
        {
          on_pressed();
          return true;
        }

        void mouse_move(double x, double y)
        {
        }

        void mouse_release(double x, double y)
        {
        }

        int press_count{0};

        std::function<void()> on_pressed = [] { };
    };

    struct ins
    {
        halp::dynamic_audio_bus<"Input", double> audio;

        halp::knob_f32<"Volume", halp::range{.min = 0., .max = 5., .init = 1.}> volume;
        halp::hslider_f32<"Level", halp::range{.min = -10, .max = 10, .init = 0}> level;
        halp::hslider_f32<"Doppler amount", halp::range{.min = -10, .max = 10, .init = 0}> dopler;
        halp::hslider_f32<"Concentration", halp::range{.min = -10, .max = 10, .init = 0}> concentration;
        halp::hslider_f32<"Dst. amount", halp::range{.min = -10, .max = 10, .init = 0}> dstAmount;
    } inputs;

    struct outs
    {
        halp::dynamic_audio_bus<"Output", double> audio;
    } outputs;

    struct ui_to_processor
    {
        int test;
    };

    struct processor_to_ui
    {
        int test_back;
    };

    struct ui {
        halp_meta(name, "Main")
        halp_meta(layout, halp::layouts::hbox)
        halp_meta(background, "background.svg")
        halp_meta(width, 900)
        halp_meta(height, 300)
        halp_meta(font, "Inconsolata")

        struct bus {
            void init(ui& ui)
            {
                ui.test.button.on_pressed = [&]{
                    fprintf(stderr, "Sending message from UI thread !\n");
                    this->send_message(ui_to_processor{.test = 1});
                };
            }

            static void process_message(ui& self, processor_to_ui msg)
            {
                fprintf(stderr, "Got message in ui thread !\n");
                self.test.button.press_count++;
            }

            std::function<void(ui_to_processor)> send_message;

        };

        struct {
            halp_meta(layout, halp::layouts::container)
            halp::custom_actions_item<custom_button> button{
                .x = 10
                , .y = 10
            };
        } test;

        struct {
            halp_meta(name, "Mosca")
            halp_meta(layout, halp::layouts::vbox)
            halp_meta(background, halp::colors::mid)

            halp::custom_item<custom_mosca, &ins::level> widget{{.x = 500, .y = 920}};
        } mosca;

        struct {
            halp_meta(name, "Panel")
            halp_meta(layout, halp::layouts::vbox)
            halp_meta(width, 300)
            halp_meta(height, 300)

            halp::item<&ins::volume> w0;
            halp::item<&ins::level> w1;
            halp::item<&ins::dopler> w2;
            halp::item<&ins::concentration> w3;
            halp::item<&ins::dstAmount> w4;
        } panel;
    };

    void prepare(halp::setup info) { previous_values.resize(info.input_channels); }

    std::function<void(processor_to_ui)> send_message;

    void process_message(const ui_to_processor& msg)
    {
        fprintf(stderr, "Got message in processing thread !\n");
        send_message(processor_to_ui{.test_back = 1});
    }

    void operator()(halp::tick t)
    {
      // Process the input buffer
      for (int i = 0; i < inputs.audio.channels; i++)
      {
        auto* in = inputs.audio[i];
        auto* out = outputs.audio[i];

        float& prev = this->previous_values[i];

        for (int j = 0; j < t.frames; j++)
        {
          out[j] = inputs.volume * in[j];
          prev = out[j];
        }
      }
    }

  private:
    // Here we have some state which depends on the host configuration (number of channels, etc).
    std::vector<float> previous_values{};
};
}
