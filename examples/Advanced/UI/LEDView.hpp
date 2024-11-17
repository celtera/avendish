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
namespace ao
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
    struct : halp::val_port<"RGB", std::vector<int>>
    {
      enum widget
      {
        control
      };
    } values;
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

    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
    {
      setAcceptedMouseButtons({});

      const Process::PortFactoryList& portFactory
          = doc.app.interfaces<Process::PortFactoryList>();

      auto inl = static_cast<Process::ControlInlet*>(process.inlets().front());

      auto fact = portFactory.get(inl->concreteKey());
      auto port = fact->makePortItem(*inl, doc, this, this);
      port->setPos(0, 5);

      connect(
          inl, &Process::ControlInlet::executionValueChanged, this,
          [this](const ossia::value& v) {
        if(auto list = v.target<std::vector<ossia::value>>())
        {
          m_pixels.clear();
          auto& vec = *list;
          for(int i = 0, N = vec.size() - 2; i < N; i += 3)
          {
            const auto r = std::clamp(ossia::convert<int>(vec[i]), 0, 255);
            const auto g = std::clamp(ossia::convert<int>(vec[i + 1]), 0, 255);
            const auto b = std::clamp(ossia::convert<int>(vec[i + 2]), 0, 255);
            m_pixels.push_back(QColor::fromRgb(r, g, b));
          }
          update();
        }
      });
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
