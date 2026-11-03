#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <Effect/EffectLayer.hpp>
#include <Effect/EffectLayout.hpp>
#include <Process/Process.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <avnd/concepts/painter.hpp>
#include <avnd/concepts/processor.hpp>
#include <avnd/wrappers/controls.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/format_value.hpp>
#include <ossia/network/value/value_conversion.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsSceneHoverEvent>
#include <QPainter>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>
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
  RGB01,
  RGB16
};

//! The order in which the colour components of a pixel arrive: WS2812 strips
//! take GRB, others BGR.
enum class LEDOrder
{
  RGB,
  GRB,
  BGR,
  RBG,
  GBR,
  BRG
};

//! How the pixels are wired across rows and columns.
enum class LEDLayout
{
  Rows,
  SerpentineRows,
  Columns,
  SerpentineColumns
};

//! Values per pixel in a mode.
inline int ledStride(LEDInputMode m) noexcept
{
  switch(m)
  {
    case RGBW:
      return 4;
    case Lightness01:
    case Lightness8bit:
      return 1;
    default:
      return 3;
  }
}

inline LEDInputMode ledModeFromName(std::string_view s) noexcept
{
  if(s == "RGBW")
    return RGBW;
  if(s == "Lightness01")
    return Lightness01;
  if(s == "Lightness8bit")
    return Lightness8bit;
  if(s == "RGB01")
    return RGB01;
  if(s == "RGB16")
    return RGB16;
  return RGB;
}

/**
 * The colours of the pixels in `values`: components read in `order`,
 * scaled from the mode's range, and raised to 1/gamma for display (values
 * meant for an LED with a gamma of 2.2 look right on screen with 2.2).
 * A white channel lights the pixel with a warm white.
 */
inline void ledColors(
    std::vector<QColor>& out, const std::vector<float>& values, LEDInputMode mode,
    LEDOrder order, float gamma)
{
  const int stride = ledStride(mode);
  const float full = mode == RGB01 || mode == Lightness01 ? 1.f
                     : mode == RGB16                     ? 65535.f
                                                          : 255.f;
  // Index of r, g and b in an incoming triple
  static constexpr int index[6][3]{{0, 1, 2}, {1, 0, 2}, {2, 1, 0},
                                   {0, 2, 1}, {2, 0, 1}, {1, 2, 0}};
  const auto& idx = index[std::clamp(int(order), 0, 5)];
  const float inv_gamma = 1.f / std::max(gamma, 0.05f);
  auto shade = [&](float v) {
    return std::pow(std::clamp(v / full, 0.f, 1.f), inv_gamma);
  };

  const int n = int(values.size()) / stride;
  out.resize(n);
  for(int p = 0; p < n; p++)
  {
    const float* v = values.data() + p * stride;
    if(stride == 1)
    {
      const float l = shade(v[0]);
      out[p] = QColor::fromRgbF(l, l, l);
      continue;
    }
    float r = shade(v[idx[0]]), g = shade(v[idx[1]]), b = shade(v[idx[2]]);
    if(stride == 4)
    {
      const float w = shade(v[3]);
      r = std::min(1.f, r + w);
      g = std::min(1.f, g + w * 0.84f);
      b = std::min(1.f, b + w * 0.67f);
    }
    out[p] = QColor::fromRgbF(r, g, b);
  }
}

inline std::vector<QColor> ledColors(
    const std::vector<float>& values, LEDInputMode mode, LEDOrder order, float gamma)
{
  std::vector<QColor> out;
  ledColors(out, values, mode, order, gamma);
  return out;
}

//! Column and row of the pixel at `index` in a grid `cols` wide, `rows` high.
inline std::pair<int, int> ledCell(int index, int cols, int rows, LEDLayout l) noexcept
{
  cols = std::max(cols, 1);
  rows = std::max(rows, 1);
  switch(l)
  {
    case LEDLayout::SerpentineRows: {
      const int r = index / cols, c = index % cols;
      return {r % 2 ? cols - 1 - c : c, r};
    }
    case LEDLayout::Columns:
      return {index / rows, index % rows};
    case LEDLayout::SerpentineColumns: {
      const int c = index / rows, r = index % rows;
      return {c, c % 2 ? rows - 1 - r : r};
    }
    default:
      return {index % cols, index / cols};
  }
}

//! Universe and 1-based slot of the first value of a pixel, filling each
//! universe with whole pixels only, as pixel mappers do (170 RGB pixels).
inline std::pair<int, int> ledSlot(int pixel, int stride) noexcept
{
  const int per_universe = std::max(1, 512 / stride);
  return {pixel / per_universe + 1, (pixel % per_universe) * stride + 1};
}

struct LEDView
{
  halp_meta(name, "LED View")
  halp_meta(c_name, "led_view")
  halp_meta(category, "Monitoring/Visuals")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/led-view.html")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Visualizes an array of [r,g,b,r,g,b,...] pixels")
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
    halp::xy_spinboxes_i32<"Size", halp::range{0, 512, 0}> size;

    struct : halp::combobox_t<"Order", LEDOrder>
    {
      halp_meta(description, "Order of the colour components of each pixel")
      struct range
      {
        std::string_view values[6]{"RGB", "GRB", "BGR", "RBG", "GBR", "BRG"};
        LEDOrder init{LEDOrder::RGB};
      };
    } order;
    struct : halp::combobox_t<"Layout", LEDLayout>
    {
      halp_meta(description, "How the pixels run across the grid; serpentine wiring turns back at each row or column")
      struct range
      {
        std::string_view values[4]{"Rows", "Serpentine rows", "Columns", "Serpentine columns"};
        LEDLayout init{LEDLayout::Rows};
      };
    } layout;
    struct : halp::spinbox_f32<"Gamma", halp::range{0.2, 4., 1.}>
    {
      halp_meta(description, "Gamma of the LEDs: 2.2 shows values corrected for LEDs as they will look")
    } gamma;
    struct : halp::toggle<"Universes">
    {
      halp_meta(description, "Mark where each 512-slot DMX universe starts")
    } universes;
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
    std::vector<float> m_values;
    std::vector<QColor> m_pixels;

    //! The settings: what the model holds, then whatever playback sends to
    //! them. Invalid when the process has no such inlet.
    enum Setting
    {
      ModeSetting,
      SizeSetting,
      OrderSetting,
      LayoutSetting,
      GammaSetting,
      UniversesSetting,
      SettingCount
    };
    std::array<ossia::value, SettingCount> m_settings;

    std::optional<QPointF> m_hover;
    int m_hovered{-1};

    Layer(
        const Process::ProcessModel& process, const Process::Context& doc,
        QGraphicsItem* parent)
        : Process::EffectLayerView{parent}
    {
      setAcceptedMouseButtons({});
      setAcceptHoverEvents(true);

      const Process::PortFactoryList& portFactory
          = doc.app.interfaces<Process::PortFactoryList>();

      auto led_inl = static_cast<Process::ControlInlet*>(process.inlets()[0]);
      // Absent from processes which have not been reloaded since they were added
      auto control = [&](std::size_t i) -> const Process::ControlInlet* {
        return i < process.inlets().size()
                   ? qobject_cast<Process::ControlInlet*>(process.inlets()[i])
                   : nullptr;
      };

      auto fact = portFactory.get(led_inl->concreteKey());
      auto port = fact->makePortItem(*led_inl, doc, this, this);
      // Centered on the top strip, where score puts the first inlet of its
      // own monitoring views
      port->setPos(0, 3);

      // An edit in the inspector and a value sent through a cable while
      // playing both replace the setting; the latest one wins
      for(int i = 0; i < SettingCount; i++)
      {
        auto* c = control(i + 1);
        if(!c)
          continue;
        m_settings[i] = c->value();
        auto set = [this, i](const ossia::value& v) {
          m_settings[i] = v;
          rebuild();
        };
        connect(c, &Process::ControlInlet::valueChanged, this, set);
        connect(c, &Process::ControlInlet::executionValueChanged, this, set);
      }

      connect(
          led_inl, &Process::ControlInlet::executionValueChanged, this,
          [this](const ossia::value& v) {
        if(auto list = v.target<std::vector<ossia::value>>())
        {
          m_values.resize(list->size());
          for(std::size_t i = 0; i < list->size(); i++)
            m_values[i] = ossia::convert<float>((*list)[i]);
          rebuild();
        }
      });
    }

    const ossia::value& setting(Setting s) const noexcept { return m_settings[s]; }

    LEDInputMode mode() const
    {
      // The model holds the name; a cable may send the index as well
      const auto& v = setting(ModeSetting);
      if(auto str = v.target<std::string>())
        return ledModeFromName(*str);
      return v.valid() ? LEDInputMode(std::clamp(ossia::convert<int>(v), 0, int(RGB16)))
                       : RGB;
    }
    ossia::vec2f gridSize() const
    {
      const auto& v = setting(SizeSetting);
      return v.valid() ? ossia::convert<ossia::vec2f>(v) : ossia::vec2f{};
    }
    template <typename E>
    E enumOf(Setting s, E fallback, int last) const
    {
      const auto& v = setting(s);
      return v.valid() ? E(std::clamp(ossia::convert<int>(v), 0, last)) : fallback;
    }
    float gamma() const
    {
      const auto& v = setting(GammaSetting);
      return v.valid() ? ossia::convert<float>(v) : 1.f;
    }
    bool universes() const
    {
      const auto& v = setting(UniversesSetting);
      return v.valid() && ossia::convert<bool>(v);
    }

    void rebuild()
    {
      ledColors(m_pixels, m_values, mode(), enumOf(OrderSetting, LEDOrder::RGB, 5), gamma());
      if(m_hover)
        m_hovered = indexAt(*m_hover);
      update();
    }

    void reset()
    {
      m_values.clear();
      m_pixels.clear();
      m_hovered = -1;
      update();
    }

    struct Grid
    {
      int cols{}, rows{};
      double cell{};
    };

    //! The grid the pixels are drawn on: the given size, or as square as fits
    Grid grid() const
    {
      const int n = int(m_pixels.size());
      const auto r = boundingRect();
      const auto sz = gridSize();
      Grid g;
      if(sz[0] > 0 && sz[1] > 0)
      {
        g.cols = int(sz[0]);
        g.rows = std::max(int(sz[1]), (n + g.cols - 1) / g.cols);
      }
      else if(sz[0] > 0)
      {
        g.cols = int(sz[0]);
        g.rows = (n + g.cols - 1) / g.cols;
      }
      else
      {
        // As many columns as keep cells square in the available area
        const double aspect = std::max(r.width(), 1.) / std::max(r.height() - 14, 1.);
        g.cols = std::max(1, int(std::ceil(std::sqrt(n * aspect))));
        g.rows = (n + g.cols - 1) / g.cols;
      }
      g.rows = std::max(g.rows, 1);
      g.cell = std::max(
          1., std::min(r.width() / g.cols, (r.height() - 14) / g.rows));
      return g;
    }

    QRectF cellRect(const Grid& g, int index) const
    {
      const auto [c, r] = ledCell(index, g.cols, g.rows, enumOf(LayoutSetting, LEDLayout::Rows, 3));
      const double gap = g.cell >= 6 ? 1. : 0.;
      return QRectF(c * g.cell, 14 + r * g.cell, g.cell - gap, g.cell - gap);
    }

    //! The pixel under `pos`, or -1.
    int indexAt(QPointF pos) const
    {
      const auto g = grid();
      for(int i = 0; i < int(m_pixels.size()); i++)
        if(cellRect(g, i).contains(pos))
          return i;
      return -1;
    }

    void hoverMoveEvent(QGraphicsSceneHoverEvent* ev) override
    {
      m_hover = ev->pos();
      if(const int i = indexAt(*m_hover); i != m_hovered)
      {
        m_hovered = i;
        update();
      }
    }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override
    {
      m_hover.reset();
      if(m_hovered >= 0)
      {
        m_hovered = -1;
        update();
      }
    }

    void paint_impl(QPainter* p) const override
    {
      if(m_pixels.empty())
        return;

      const auto g = grid();
      const int stride = ledStride(mode());
      const bool universes = this->universes();
      p->setPen(Qt::NoPen);
      const int hovered = m_hovered < int(m_pixels.size()) ? m_hovered : -1;
      for(int i = 0; i < int(m_pixels.size()); i++)
      {
        const auto rect = cellRect(g, i);
        p->fillRect(rect, m_pixels[i]);
        if(universes && i > 0 && ledSlot(i, stride).first != ledSlot(i - 1, stride).first)
        {
          // A universe starts here
          p->fillRect(QRectF(rect.left() - 1, rect.top(), 2, rect.height()), QColor(255, 200, 0));
          QFont f = p->font();
          f.setPixelSize(9);
          p->setFont(f);
          p->setPen(QColor(255, 200, 0));
          p->drawText(rect.topLeft() + QPointF{2, 9}, QString("U%1").arg(ledSlot(i, stride).first));
          p->setPen(Qt::NoPen);
        }
      }

      if(hovered >= 0)
      {
        const auto rect = cellRect(g, hovered);
        p->setPen(QPen(Qt::white, 1));
        p->setBrush(Qt::NoBrush);
        p->drawRect(rect);
        const auto [u, slot] = ledSlot(hovered, stride);
        QString vals;
        for(int k = 0; k < stride && hovered * stride + k < int(m_values.size()); k++)
          vals += (k ? " " : "") + QString::number(m_values[hovered * stride + k], 'g', 4);
        const QString text = QStringLiteral("#%1  U%2 slot %3  [%4]").arg(hovered).arg(u).arg(slot).arg(vals);
        QFont f = p->font();
        f.setPixelSize(10);
        p->setFont(f);
        const QRectF box{0, 0, boundingRect().width(), 13};
        p->fillRect(box, QColor(0, 0, 0, 180));
        p->setPen(QColor(230, 230, 230));
        p->drawText(box.adjusted(3, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, text);
      }
    }
  };
};
}
