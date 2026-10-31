#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>
#include <Process/Commands/SetControlValue.hpp>
#include <Process/Process.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <avnd/concepts/painter.hpp>
#include <avnd/concepts/processor.hpp>
#include <avnd/wrappers/controls.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/format_value.hpp>
#include <ossia/network/value/value_conversion.hpp>
#include <score/application/GUIApplicationContext.hpp>

#include <score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>

#include <QElapsedTimer>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QLineF>
#include <QStyleHints>
#include <QPainter>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <optional>
#include <vector>
namespace uo
{
//! Where \p v lands in [0;1] across the range [\p min ; \p max].
//!
//! A range whose min is greater than its max reads the other way round rather
//! than being rejected: that is how an axis is inverted, and it is the only way
//! to point Y downwards.
inline double axisRatio(double v, double min, double max) noexcept
{
  return (v - min) / (max - min);
}

//! A range is unusable when its two ends meet, not when they are the wrong way
//! round.
inline bool axisUsable(double min, double max) noexcept
{
  return std::abs(max - min) > 1e-6;
}

//! A colour from a value: [r, g, b(, a)] in 0..1, or a single grey level.
inline std::optional<QColor> colorOf(const ossia::value& v)
{
  switch(v.get_type())
  {
    case ossia::val_type::VEC3F: {
      auto c = v.get<ossia::vec3f>();
      return QColor::fromRgbF(
          std::clamp(c[0], 0.f, 1.f), std::clamp(c[1], 0.f, 1.f), std::clamp(c[2], 0.f, 1.f));
    }
    case ossia::val_type::VEC4F: {
      auto c = v.get<ossia::vec4f>();
      return QColor::fromRgbF(
          std::clamp(c[0], 0.f, 1.f), std::clamp(c[1], 0.f, 1.f), std::clamp(c[2], 0.f, 1.f),
          std::clamp(c[3], 0.f, 1.f));
    }
    case ossia::val_type::LIST: {
      auto& l = *v.target<std::vector<ossia::value>>();
      if(l.size() >= 3)
        return QColor::fromRgbF(
            std::clamp(ossia::convert<float>(l[0]), 0.f, 1.f),
            std::clamp(ossia::convert<float>(l[1]), 0.f, 1.f),
            std::clamp(ossia::convert<float>(l[2]), 0.f, 1.f));
      return std::nullopt;
    }
    case ossia::val_type::FLOAT:
    case ossia::val_type::INT: {
      const float g = std::clamp(ossia::convert<float>(v), 0.f, 1.f);
      return QColor::fromRgbF(g, g, g);
    }
    default:
      return std::nullopt;
  }
}

//! The points of a value: [x, y, x, y, ...] or a list of 2D vectors.
inline std::vector<QPointF> pointsOf(const ossia::value& v)
{
  std::vector<QPointF> pts;
  auto list = v.target<std::vector<ossia::value>>();
  if(!list || list->empty())
    return pts;
  auto& vec = *list;
  if(vec[0].target<float>() || vec[0].target<int>() || vec[0].target<bool>())
  {
    for(std::size_t i = 0; i + 1 < vec.size(); i += 2)
      pts.push_back(QPointF{ossia::convert<float>(vec[i]), ossia::convert<float>(vec[i + 1])});
  }
  else
  {
    for(auto& e : vec)
    {
      auto [x, y] = ossia::convert<ossia::vec2f>(e);
      pts.push_back(QPointF{x, y});
    }
  }
  return pts;
}

struct Point2DView
{
  halp_meta(name, "Point2D View")
  halp_meta(c_name, "point2d_view")
  halp_meta(category, "Monitoring/Control")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/point2d-view.html")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Visualizes an array of [x,y,x,y,...] positions")
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

    struct : halp::val_port<"Colors", std::vector<ossia::value>>
    {
      halp_meta(description, "One colour per point: [r, g, b] in 0 to 1, or a grey level")
      enum widget
      {
        control
      };
    } colors;
    struct : halp::spinbox_i32<"Trail", halp::range{0, 200, 0}>
    {
      halp_meta(description, "How many previous positions of each point stay drawn")
    } trail;
    struct : halp::spinbox_f32<"Point size", halp::range{1., 40., 3.}>
    {
    } point_size;
    struct : halp::toggle<"Labels">
    {
      halp_meta(description, "Number each point")
    } labels;
    struct : halp::toggle<"Auto range">
    {
      halp_meta(description, "Fit the range to the points received instead of Min and Max")
    } auto_range;
    struct : halp::toggle<"Keep aspect">
    {
      halp_meta(description, "Same scale on both axes")
    } aspect;
  } inputs;

  struct Layer : public Process::EffectLayerView
  {
  public:
    std::vector<QPointF> m_points;
    std::vector<QColor> m_colors;
    //! Previous point sets, newest last
    std::deque<std::vector<QPointF>> m_trail;
    //! What the points received since execution started span
    QRectF m_seen;

    // The settings: what the model holds, then whatever playback sends
    float min_x{0.f}, min_y{0.f}, max_x{1.f}, max_y{1.f};
    int m_trail_length{0};
    float m_point_size{3.f};
    bool m_labels{false};
    bool m_auto_range{false};
    bool m_keep_aspect{false};
    QElapsedTimer m_lastPress;
    QPoint m_lastPressPos;

    //! Min X, Min Y, Max X, Max Y, which a double click fits to the points
    std::array<const Process::ControlInlet*, 4> m_bounds{};
    const score::CommandStackFacade& m_stack;

    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
        , m_stack{doc.commandStack}
    {
      // For the double click; a press goes on to the node, to select or move it
      setAcceptedMouseButtons(Qt::LeftButton);

      const Process::PortFactoryList& portFactory
          = doc.app.interfaces<Process::PortFactoryList>();

      auto xy_inl = static_cast<Process::ControlInlet*>(process.inlets()[0]);
      auto fact = portFactory.get(xy_inl->concreteKey());
      auto port = fact->makePortItem(*xy_inl, doc, this, this);
      port->setPos(0, 5);

      // Absent from processes which have not been reloaded since they were added
      auto control = [&](std::size_t i) -> const Process::ControlInlet* {
        return i < process.inlets().size()
                   ? qobject_cast<Process::ControlInlet*>(process.inlets()[i])
                   : nullptr;
      };

      // A setting is what the model holds, then whatever playback sends to
      // it; an edit in the inspector replaces it again
      auto bind = [&]<typename T>(std::size_t index, T& field) {
        auto* c = control(index);
        if(!c)
          return;
        field = ossia::convert<T>(c->value());
        auto set = [this, &field](const ossia::value& v) {
          field = ossia::convert<T>(v);
          update();
        };
        connect(c, &Process::ControlInlet::valueChanged, this, set);
        connect(c, &Process::ControlInlet::executionValueChanged, this, set);
      };
      for(int i = 0; i < 4; i++)
        m_bounds[i] = control(1 + i);
      bind(1, min_x);
      bind(2, min_y);
      bind(3, max_x);
      bind(4, max_y);
      bind(6, m_trail_length);
      bind(7, m_point_size);
      bind(8, m_labels);
      bind(9, m_auto_range);
      bind(10, m_keep_aspect);

      // The auto range fits one playback
      connect(&process, &Process::ProcessModel::startExecution, this, [this] { reset(); });

      if(auto* colors = control(5))
        connect(
            colors, &Process::ControlInlet::executionValueChanged, this,
            [this](const ossia::value& v) {
          m_colors.clear();
          if(auto list = v.target<std::vector<ossia::value>>())
            for(auto& c : *list)
              m_colors.push_back(colorOf(c).value_or(QColor{}));
          update();
        });

      connect(
          xy_inl, &Process::ControlInlet::executionValueChanged, this,
          [this](const ossia::value& v) {
        auto pts = pointsOf(v);
        if(pts.empty())
          return;
        const int trail = std::max(0, m_trail_length);
        if(trail > 0 && !m_points.empty())
        {
          m_trail.push_back(std::move(m_points));
          while(int(m_trail.size()) > trail)
            m_trail.pop_front();
        }
        else if(trail == 0)
          m_trail.clear();
        m_points = std::move(pts);
        for(auto& pt : m_points)
          m_seen = m_seen.isNull() ? QRectF{pt, QSizeF{1e-9, 1e-9}} : m_seen.united(QRectF{pt, QSizeF{1e-9, 1e-9}});
        update();
      });
    }

    //! A first press goes on to the node, to select or move it. The scene
    //! hands the second press of a double click to whoever took the first, so
    //! the double click is recognised here, from the presses.
    void mousePressEvent(QGraphicsSceneMouseEvent* ev) override
    {
      const bool second = m_lastPress.isValid()
                          && m_lastPress.elapsed() < QGuiApplication::styleHints()->mouseDoubleClickInterval()
                          && QLineF(ev->screenPos(), m_lastPressPos).length() < 6.;
      m_lastPress.restart();
      m_lastPressPos = ev->screenPos();
      if(!second)
      {
        ev->ignore();
        return;
      }
      ev->accept();
      m_lastPress.invalidate();
      fitToPoints();
    }

    //! The range is set to what the points shown span, as one undoable edit
    void fitToPoints()
    {
      QRectF r;
      auto add = [&](const std::vector<QPointF>& pts) {
        for(auto& pt : pts)
          r = r.isNull() ? QRectF{pt, QSizeF{1e-9, 1e-9}} : r.united(QRectF{pt, QSizeF{1e-9, 1e-9}});
      };
      add(m_points);
      for(auto& t : m_trail)
        add(t);
      if(r.isNull() || !std::all_of(m_bounds.begin(), m_bounds.end(), [](auto* c) { return c; }))
        return;
      // A margin, and some room around a single point
      const double mx = std::max(r.width() * 0.05, 1e-3), my = std::max(r.height() * 0.05, 1e-3);
      const float v[4]{
          float(r.left() - mx), float(r.top() - my), float(r.right() + mx), float(r.bottom() + my)};
      MultiOngoingCommandDispatcher disp{m_stack};
      for(int i = 0; i < 4; i++)
        disp.submit(new Process::SetControlValue{*m_bounds[i], v[i]});
      disp.commit<Process::SetControlValues>();
    }

    void reset()
    {
      m_points.clear();
      m_trail.clear();
      m_seen = {};
      update();
    }

    struct Range
    {
      double x0, x1, y0, y1;
    };
    Range range() const
    {
      if(m_auto_range && !m_seen.isNull())
      {
        const double mx = std::max(m_seen.width() * 0.05, 1e-6);
        const double my = std::max(m_seen.height() * 0.05, 1e-6);
        return {m_seen.left() - mx, m_seen.right() + mx, m_seen.top() - my, m_seen.bottom() + my};
      }
      return {min_x, max_x, min_y, max_y};
    }

    void paint_impl(QPainter* p) const override
    {
      auto [x0, x1, y0, y1] = range();
      if(!axisUsable(x0, x1) || !axisUsable(y0, y1))
        return;

      p->setRenderHint(QPainter::RenderHint::Antialiasing, true);
      auto& skin = score::Skin::instance();
      QRectF plot = boundingRect().adjusted(1, 1, -1, -1);
      if(m_keep_aspect)
      {
        // Same units per pixel on both axes, centred
        const double ux = std::abs(x1 - x0) / plot.width(), uy = std::abs(y1 - y0) / plot.height();
        const double u = std::max(ux, uy);
        const QSizeF sz{std::abs(x1 - x0) / u, std::abs(y1 - y0) / u};
        plot = QRectF{plot.center() - QPointF{sz.width() / 2, sz.height() / 2}, sz};
      }
      const auto w = plot.width();
      const auto h = plot.height();
      auto map = [&](QPointF pt) {
        return QPointF{
            plot.left() + w * axisRatio(pt.x(), x0, x1),
            plot.top() + h * (1. - axisRatio(pt.y(), y0, y1))};
      };

      // Quarter grid
      p->setBrush(skin.NoBrush);
      p->setPen(QPen(QColor(255, 255, 255, 20), 1));
      for(int i = 1; i < 4; i++)
      {
        p->drawLine(QPointF{plot.left() + w * i / 4, plot.top()}, QPointF{plot.left() + w * i / 4, plot.bottom()});
        p->drawLine(QPointF{plot.left(), plot.top() + h * i / 4}, QPointF{plot.right(), plot.top() + h * i / 4});
      }

      // The axes, where 0 is in view: brighter than the grid, so that a range
      // straddling zero, or one running backwards, says which way round it is
      {
        const auto ox = axisRatio(0., x0, x1);
        const auto oy = axisRatio(0., y0, y1);
        p->setPen(QPen(QColor(255, 255, 255, 90), 1));
        if(ox >= 0. && ox <= 1.)
        {
          const double x = plot.left() + w * ox;
          p->drawLine(QPointF{x, plot.top()}, QPointF{x, plot.bottom()});
        }
        if(oy >= 0. && oy <= 1.)
        {
          const double y = plot.top() + h * (1. - oy);
          p->drawLine(QPointF{plot.left(), y}, QPointF{plot.right(), y});
        }
      }

      // The range at the ends of the axes: clear of the inlet on the top left
      // and of the resize corner on the bottom right
      if(w > 80 && h > 40)
      {
        QFont f = p->font();
        f.setPixelSize(9);
        p->setFont(f);
        p->setPen(QColor(150, 150, 150));
        p->drawText(QRectF(plot.left() + 3, plot.bottom() - 12, 60, 11), Qt::AlignLeft, QString::number(x0, 'g', 3));
        p->drawText(QRectF(plot.right() - 74, plot.bottom() - 12, 60, 11), Qt::AlignRight, QString::number(x1, 'g', 3));
        p->drawText(QRectF(plot.right() - 63, plot.top() + 2, 60, 11), Qt::AlignRight, QString::number(y1, 'g', 3));
      }

      const double side = std::max(1.f, m_point_size);
      auto colorAt = [&](std::size_t i) {
        return i < m_colors.size() && m_colors[i].isValid() ? m_colors[i] : skin.Base1.color();
      };

      // Trails: each point's previous positions, fading out
      p->setBrush(skin.NoBrush);
      const int nt = int(m_trail.size());
      for(int t = 0; t < nt; t++)
      {
        const auto& from = m_trail[t];
        const auto& to = t + 1 < nt ? m_trail[t + 1] : m_points;
        for(std::size_t i = 0; i < from.size() && i < to.size(); i++)
        {
          QColor c = colorAt(i);
          c.setAlphaF(0.8 * (t + 1) / (nt + 1));
          p->setPen(QPen(c, std::max(1., side / 2.)));
          p->drawLine(map(from[i]), map(to[i]));
        }
      }

      p->setPen(skin.NoPen);
      const bool labels = m_labels;
      for(std::size_t i = 0; i < m_points.size(); i++)
      {
        const auto c = map(m_points[i]);
        p->setBrush(colorAt(i));
        p->drawEllipse(c, side, side);
        if(labels)
        {
          QFont f = p->font();
          f.setPixelSize(9);
          p->setFont(f);
          p->setPen(QColor(220, 220, 220));
          p->drawText(c + QPointF{side + 2, -side}, QString::number(i));
          p->setPen(skin.NoPen);
        }
      }
      p->setRenderHint(QPainter::RenderHint::Antialiasing, false);
    }
  };
};
}
