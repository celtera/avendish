#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>
#include <Process/Process.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <avnd/concepts/painter.hpp>
#include <avnd/concepts/processor.hpp>
#include <avnd/wrappers/controls.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/format_value.hpp>
#include <ossia/network/value/value_conversion.hpp>
#include <score/application/GUIApplicationContext.hpp>

#include <QPainter>
namespace ao
{
struct Point2DView
{
  halp_meta(name, "Point2D View")
  halp_meta(c_name, "point2d_view")
  halp_meta(category, "Monitoring")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/point2d-view.html")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Visualizes an array of [x,y,x,y,...] positions")
  halp_meta(uuid, "898078f0-b825-4c29-a997-dc856e351a97")
  halp_flag(fully_custom_item);

  struct ins
  {
    struct : halp::val_port<"XY", std::vector<ossia::value>>
    {
      enum widget
      {
        control
      };
    } values;
    halp::spinbox_f32<"Min X", halp::free_range_min<float>> min_x;
    halp::spinbox_f32<"Min Y", halp::free_range_min<float>> min_y;
    halp::spinbox_f32<"Max X", halp::free_range_max<float>> max_x;
    halp::spinbox_f32<"Max Y", halp::free_range_max<float>> max_y;
  } inputs;

  struct Layer : public Process::EffectLayerView
  {
  public:
    std::vector<QPointF> m_points;
    float min_x{0.f}, min_y{0.f}, max_x{1.f}, max_y{1.f};

    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
    {
      setAcceptedMouseButtons({});

      const Process::PortFactoryList& portFactory
          = doc.app.interfaces<Process::PortFactoryList>();

      auto xy_inl = static_cast<Process::ControlInlet*>(process.inlets()[0]);
      auto fact = portFactory.get(xy_inl->concreteKey());
      auto port = fact->makePortItem(*xy_inl, doc, this, this);
      port->setPos(0, 5);

      connect(
          static_cast<Process::ControlInlet*>(process.inlets()[1]),
          &Process::ControlInlet::executionValueChanged, this,
          [this](const ossia::value& v) {
        min_x = ossia::convert<float>(v);
        update();
      });
      connect(
          static_cast<Process::ControlInlet*>(process.inlets()[2]),
          &Process::ControlInlet::executionValueChanged, this,
          [this](const ossia::value& v) {
        min_y = ossia::convert<float>(v);
        update();
      });
      connect(
          static_cast<Process::ControlInlet*>(process.inlets()[3]),
          &Process::ControlInlet::executionValueChanged, this,
          [this](const ossia::value& v) {
        max_x = ossia::convert<float>(v);
        update();
      });
      connect(
          static_cast<Process::ControlInlet*>(process.inlets()[4]),
          &Process::ControlInlet::executionValueChanged, this,
          [this](const ossia::value& v) {
        max_y = ossia::convert<float>(v);
        update();
      });

      connect(
          xy_inl, &Process::ControlInlet::executionValueChanged, this,
          [this](const ossia::value& v) {
        if(auto list = v.target<std::vector<ossia::value>>())
        {
          m_points.clear();
          auto& vec = *list;
          if(vec.empty())
            return;
          if(vec[0].target<float>() || vec[0].target<int>() || vec[0].target<bool>())
          {
            for(int i = 0, N = vec.size(); i + 1 < N; i += 2)
            {
              float x = ossia::convert<float>(vec[i]);
              float y = ossia::convert<float>(vec[i + 1]);
              m_points.push_back(QPointF{x, y});
            }
          }
          else
          {
            for(int i = 0, N = vec.size(); i < N; i++)
            {
              auto [x, y] = ossia::convert<ossia::vec2f>(vec[i]);
              m_points.push_back(QPointF{x, y});
            }
          }

          update();
        }
      });
    }

    void reset()
    {
      m_points.clear();
      update();
    }

    void paint_impl(QPainter* p) const override
    {
      static constexpr auto side = 4.;
      if(m_points.empty())
        return;

      float scalex = max_x - min_x;
      float scaley = max_y - min_y;
      if(scalex < 0.000001f || scaley < 0.000001f)
        return;

      const auto rect = boundingRect();
      const auto w = rect.width();
      const auto h = rect.height();
      auto& skin = score::Skin::instance();
      p->fillRect(boundingRect(), skin.Background1);
      p->setPen(Qt::NoPen);
      p->setBrush(skin.Emphasis3);
      for(QPointF pix : m_points)
      {
        const auto x01 = (pix.x() - min_x) / scalex;
        const auto y01 = 1. - (pix.y() - min_y) / scaley;
        p->drawEllipse(QPointF{w * x01 - side / 2., h * y01 - side / 2.}, side, side);
      }
    }
  };
};
}
