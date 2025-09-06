#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>
#include <Process/Process.hpp>
#include <avnd/concepts/painter.hpp>
#include <avnd/concepts/processor.hpp>
#include <avnd/wrappers/controls.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/format_value.hpp>
#include <ossia/network/value/value_conversion.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/model/Skin.hpp>

#include <QPainter>
namespace uo
{
#if 0
struct led_item
{
  using item_type = custom_anim;
  static constexpr double width() { return 100.; }
  static constexpr double height() { return 10.; }

  void paint(avnd::painter auto ctx)
  {
    constexpr double side = 8.;

    // ctx.translate(10, 10);
    for(int i = 0; i < values.size() / 3; i += 3)
    {
      ctx.translate(0, 0);
      ctx.begin_path();

      ctx.set_stroke_color({.r = 0, .g = 0, .b = 0, .a = 255});
      ctx.set_fill_color(
          {.r = uint8_t(values[i]),
           .g = uint8_t(values[i + 1]),
           .b = uint8_t(values[i + 2]),
           .a = 255});
      ctx.draw_rect(-2, -8, side, side);
      ctx.fill();
      ctx.stroke();
    }

    ctx.update();
  }

  std::vector<int> values;
};
#endif

enum LEDInputMode
{
  RGB,
  RGBW,
  Lightness01,
  Lightness8bit,
};
struct LEDView
{
  halp_meta(name, "LED View")
  halp_meta(c_name, "led_view")
  halp_meta(category, "Monitoring")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/led-view.html")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Visualizes an array of [r,g,b,r,g,b,...] pixels")
  halp_meta(uuid, "C618774D-086F-4888-8404-23B6E59440F8")
  halp_flag(fully_custom_item);

  struct ins
  {
    struct : halp::val_port<"RGB", std::vector<float>>
    {
      enum widget
      {
        control
      };
    } values;

    halp::enum_t<LEDInputMode, "Mode"> mode;
  } inputs;

#if 0
  struct
  {
  } outputs;

  void operator()() { send_message(inputs.values); }

  std::function<void(std::vector<int>)> send_message;
  struct ui
  {
    halp_meta(name, "Main")
    halp_meta(layout, halp::layouts::container)
    halp_meta(width, 100)
    halp_meta(height, 20)

    halp::custom_actions_item<led_item> anim{.x = 0, .y = 0};
    struct bus
    {
      static void process_message(ui& self, std::vector<int> msg)
      {
        self.anim.values.assign(msg.begin(), msg.end());
      }
    };
  };
#endif

  struct Layer : public Process::EffectLayerView
  {
  public:
    std::vector<QColor> m_pixels;
    std::string m_mode = "RGB";
    LEDInputMode m_display_mode{};

    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
    {
      setAcceptedMouseButtons({});

      const Process::PortFactoryList& portFactory
          = doc.app.interfaces<Process::PortFactoryList>();

      auto led_inl = static_cast<Process::ControlInlet*>(process.inlets()[0]);
      auto mode_inl = static_cast<Process::ControlInlet*>(process.inlets()[1]);
      auto mode = ossia::value_to_pretty_string(mode_inl->value());

      connect(
          mode_inl, &Process::ControlInlet::executionValueChanged, this,
          [this, mode_inl](const ossia::value& v) {
        auto str = ossia::convert<std::string>(mode_inl->value());
        if(str != m_mode)
        {
          m_pixels.clear();
          m_mode = str;
          update();
        }
      });

      auto fact = portFactory.get(led_inl->concreteKey());
      auto port = fact->makePortItem(*led_inl, doc, this, this);
      port->setPos(0, 5);

      connect(
          led_inl, &Process::ControlInlet::executionValueChanged, this,
          [this](const ossia::value& v) {
        if(auto list = v.target<std::vector<ossia::value>>())
        {
          update_mode();
          m_pixels.clear();

          switch(m_display_mode)
          {
            case LEDInputMode::RGB: {
              auto& vec = *list;
              for(int i = 0, N = vec.size() - 2; i < N; i += 3)
              {
                const auto r = std::clamp(ossia::convert<float>(vec[i]), 0.f, 255.f);
                const auto g = std::clamp(ossia::convert<float>(vec[i + 1]), 0.f, 255.f);
                const auto b = std::clamp(ossia::convert<float>(vec[i + 2]), 0.f, 255.f);
                m_pixels.push_back(QColor::fromRgb(r, g, b));
              }
              break;
            }
            case LEDInputMode::RGBW: {
              auto& vec = *list;
              for(int i = 0, N = vec.size() - 3; i < N; i += 4)
              {
                const auto r = std::clamp(ossia::convert<float>(vec[i]), 0.f, 255.f);
                const auto g = std::clamp(ossia::convert<float>(vec[i + 1]), 0.f, 255.f);
                const auto b = std::clamp(ossia::convert<float>(vec[i + 2]), 0.f, 255.f);
                // FIXME w in rgbw
                // const auto w = std::clamp(ossia::convert<float>(vec[i + 3]), 0.f, 255.f);
                m_pixels.push_back(QColor::fromRgb(r, g, b));
              }
              break;
            }
            case LEDInputMode::Lightness01: {
              auto& vec = *list;
              for(int i = 0, N = vec.size(); i < N; i++)
              {
                const auto l = std::clamp(ossia::convert<float>(vec[i]), 0.f, 1.f);
                m_pixels.push_back(QColor::fromRgbF(l, l, l));
              }
              break;
            }
            case LEDInputMode::Lightness8bit: {
              auto& vec = *list;
              for(int i = 0, N = vec.size(); i < N; i++)
              {
                const auto l = std::clamp(ossia::convert<float>(vec[i]), 0.f, 255.f);
                m_pixels.push_back(QColor::fromRgb(l, l, l));
              }
              break;
            }
          }

          update();
        }
      });
    }

    void update_mode()
    {
      if(m_mode == "RGB")
        m_display_mode = LEDInputMode::RGB;
      else if(m_mode == "RGBW")
        m_display_mode = LEDInputMode::RGBW;
      else if(m_mode == "Lightness01")
        m_display_mode = LEDInputMode::Lightness01;
      else if(m_mode == "Lightness8bit")
        m_display_mode = LEDInputMode::Lightness8bit;
    }

    void reset()
    {
      m_pixels.clear();
      update();
    }

    void paint_impl(QPainter* p) const override
    {
      static constexpr auto side = 10;
      if(m_pixels.empty())
        return;

      int xpos = 0;
      int ypos = 0;
      p->setPen(Qt::NoPen);
      for(QColor pix : m_pixels)
      {
        p->fillRect(QRectF(xpos, ypos, side, side), pix);
        xpos += side + 1;
        if(xpos + side > this->boundingRect().width())
        {
          xpos = 0;
          ypos += side + 1;
        }
      }
    }
  };
};
}
