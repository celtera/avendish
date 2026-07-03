#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Software painter satisfying avnd::painter on top of canvas_ity
 * (single-header ISC software rasterizer with HTML5-canvas semantics).
 * The same paint() code as the QPainter / Canvas2D backends thus renders
 * into a plain RGBA framebuffer, portable from plug-in editors to MCUs.
 *
 * Conventions follow the other painter backends (see binding/wasm/painter.hpp):
 * draw_rect / draw_circle / ... only add subpaths; a following fill() /
 * stroke() commits them. Angles are in degrees, Qt-style.
 */

#include <avnd/binding/ui/soft/fonts.hpp>
#include <avnd/concepts/painter.hpp>

#include <canvas_ity.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace avnd::soft_ui
{
struct rgba
{
  std::uint8_t r{}, g{}, b{}, a{255};
};

static constexpr double deg2rad = 3.141592653589793 / 180.0;

class painter
{
public:
  painter(canvas_ity::canvas& c, const font_registry& fonts, bool& dirty)
      : m_c{c}
      , m_fonts{fonts}
      , m_dirty{dirty}
  {
  }

  // Custom widgets call this to request a repaint (same contract as the
  // Qt and Canvas2D backends).
  void update() noexcept { m_dirty = true; }

  // ---- Paths ----
  void begin_path() { m_c.begin_path(); }
  void close_path() { m_c.close_path(); }
  void stroke() { m_c.stroke(); }
  void fill() { m_c.fill(); }

  void move_to(double x, double y) { m_c.move_to(x, y); }
  void line_to(double x, double y) { m_c.line_to(x, y); }

  void cubic_to(double c1x, double c1y, double c2x, double c2y, double ex, double ey)
  {
    m_c.bezier_curve_to(c1x, c1y, c2x, c2y, ex, ey);
  }
  void quad_to(double x1, double y1, double x2, double y2)
  {
    m_c.quadratic_curve_to(x1, y1, x2, y2);
  }

  // Qt-style elliptic arc: (x, y, w, h) bounding box, start angle & sweep
  // length in degrees, CCW positive with 0 at 3 o'clock. canvas_ity only has
  // circular arcs, so draw a unit-circle arc under a temporary transform
  // (points are transformed as they are added; save/restore does not touch
  // the path).
  void arc_to(double x, double y, double w, double h, double start, double length)
  {
    if(w <= 0. || h <= 0.)
      return;
    const double cx = x + w / 2.0;
    const double cy = y + h / 2.0;
    const double a0 = -start * deg2rad;
    const double a1 = -(start + length) * deg2rad;
    m_c.save();
    m_c.translate(cx, cy);
    m_c.scale(w / 2.0, h / 2.0);
    m_c.arc(0.f, 0.f, 1.f, a0, a1, length > 0.0);
    m_c.restore();
  }

  // ---- Transforms ----
  void translate(double x, double y) { m_c.translate(x, y); }
  void scale(double x, double y) { m_c.scale(x, y); }
  void rotate(double deg) { m_c.rotate(deg * deg2rad); }
  void reset_transform()
  {
    m_c.set_transform(1.f, 0.f, 0.f, 1.f, 0.f, 0.f);
    if(m_base_tx != 0. || m_base_ty != 0.)
      m_c.translate(m_base_tx, m_base_ty);
  }

  // ---- Colors ----
  void set_stroke_color(rgba c)
  {
    m_c.set_color(
        canvas_ity::stroke_style, c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
  }
  void set_stroke_width(double w) { m_c.set_line_width(w); }
  void set_fill_color(rgba c)
  {
    m_c.set_color(
        canvas_ity::fill_style, c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
  }

  // ---- Text ----
  void set_font(std::string_view name) { m_font_name = name; m_font_dirty = true; }
  void set_font_size(double pt) { m_font_size = pt; m_font_dirty = true; }

  void draw_text(double x, double y, std::string_view text)
  {
    apply_font();
    m_text.assign(text);
    m_c.fill_text(m_text.c_str(), x, y);
  }

  // ---- Drawing ----
  void draw_line(double x1, double y1, double x2, double y2)
  {
    m_c.move_to(x1, y1);
    m_c.line_to(x2, y2);
  }

  void draw_rect(double x, double y, double w, double h)
  {
    m_c.rectangle(x, y, w, h);
  }

  void draw_rounded_rect(double x, double y, double w, double h, double r)
  {
    r = std::min({r, w / 2.0, h / 2.0});
    m_c.move_to(x + r, y);
    m_c.arc_to(x + w, y, x + w, y + h, r);
    m_c.arc_to(x + w, y + h, x, y + h, r);
    m_c.arc_to(x, y + h, x, y, r);
    m_c.arc_to(x, y, x + w, y, r);
    m_c.close_path();
  }

  void draw_pixmap(double x, double y, std::string_view name)
  {
    // Image registry comes with the resource story; explicit no-op for now.
    (void)x, (void)y, (void)name;
  }

  void draw_ellipse(double x, double y, double w, double h)
  {
    arc_to(x, y, w, h, 0., 360.);
  }

  void draw_circle(double cx, double cy, double r)
  {
    m_c.move_to(cx + r, cy);
    m_c.arc(cx, cy, r, 0.f, float(2. * 3.141592653589793));
  }

  void draw_bytes(
      double x, double y, double w, double h, const unsigned char* bytes, int img_w,
      int img_h)
  {
    if(!bytes || img_w <= 0 || img_h <= 0)
      return;
    m_c.draw_image(bytes, img_w, img_h, img_w * 4, x, y, w, h);
  }

  void draw_bytes(
      double x, double y, double w, double h, const float* bytes, int img_w, int img_h)
  {
    if(!bytes || img_w <= 0 || img_h <= 0)
      return;
    m_bytes.resize(size_t(img_w) * img_h * 4);
    for(size_t i = 0, n = m_bytes.size(); i < n; i++)
    {
      const float v = bytes[i];
      m_bytes[i] = (unsigned char)(v <= 0.f ? 0 : v >= 1.f ? 255 : v * 255.f + 0.5f);
    }
    m_c.draw_image(m_bytes.data(), img_w, img_h, img_w * 4, x, y, w, h);
  }

  // ---- Internal: used by the runtime to position widget-local painting ----
  void set_base_translation(double x, double y)
  {
    m_base_tx = x;
    m_base_ty = y;
    reset_transform();
  }

private:
  void apply_font()
  {
    if(!m_font_dirty)
      return;
    if(auto* f = m_fonts.find(m_font_name))
      m_c.set_font(f->bytes.data(), (int)f->bytes.size(), (float)m_font_size);
    m_font_dirty = false;
  }

  canvas_ity::canvas& m_c;
  const font_registry& m_fonts;
  bool& m_dirty;
  std::string m_font_name;
  std::string m_text;
  std::vector<unsigned char> m_bytes;
  double m_font_size{12.};
  double m_base_tx{}, m_base_ty{};
  bool m_font_dirty{true};
};

static_assert(avnd::painter<painter>);
}
