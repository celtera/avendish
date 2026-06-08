#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Canvas2D painter adapter for the WASM backend. `wasm::canvas_painter`
 * satisfies `avnd::painter` by forwarding every primitive to a JS
 * CanvasRenderingContext2D, reusing the same paint() code as the native
 * QPainter backend. draw_rect/ellipse/circle/etc. only add subpaths (Qt
 * convention); a following fill()/stroke() commits them.
 */

#include <avnd/concepts/painter.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

#if defined(__EMSCRIPTEN__)
#include <emscripten/val.h>
#endif

namespace wasm
{

struct rgba_color
{
  std::uint8_t r, g, b, a;
};

#if defined(__EMSCRIPTEN__)

class canvas_painter
{
public:
  explicit canvas_painter(emscripten::val ctx)
      : m_ctx{std::move(ctx)}
  {
  }

  [[nodiscard]] bool needs_repaint() const noexcept { return m_dirty; }
  void update() noexcept { m_dirty = true; }

  // ---- Paths ----
  void begin_path() { m_ctx.call<void>("beginPath"); }
  void close_path() { m_ctx.call<void>("closePath"); }
  void stroke() { m_ctx.call<void>("stroke"); }
  void fill() { m_ctx.call<void>("fill"); }

  void move_to(double x, double y) { m_ctx.call<void>("moveTo", x, y); }
  void line_to(double x, double y) { m_ctx.call<void>("lineTo", x, y); }

  void cubic_to(
      double c1x, double c1y, double c2x, double c2y, double ex, double ey)
  {
    m_ctx.call<void>("bezierCurveTo", c1x, c1y, c2x, c2y, ex, ey);
  }
  void quad_to(double x1, double y1, double x2, double y2)
  {
    m_ctx.call<void>("quadraticCurveTo", x1, y1, x2, y2);
  }

  // avnd/Qt arcTo: (x,y,w,h) bounding box, start & length in degrees, CCW with 0
  // at 3 o'clock. Canvas ellipse() wants a centre + radii + CW radian angles, so
  // negate the angles and draw anticlockwise.
  void arc_to(double x, double y, double w, double h, double start, double length)
  {
    constexpr double deg2rad = 3.141592653589793 / 180.0;
    const double cx = x + w / 2.0;
    const double cy = y + h / 2.0;
    const double rx = w / 2.0;
    const double ry = h / 2.0;
    const double a0 = -start * deg2rad;
    const double a1 = -(start + length) * deg2rad;
    m_ctx.call<void>(
        "ellipse", cx, cy, rx, ry, 0.0, a0, a1, length > 0.0);
  }

  void translate(double x, double y) { m_ctx.call<void>("translate", x, y); }
  void scale(double x, double y) { m_ctx.call<void>("scale", x, y); }
  void rotate(double a)
  {
    // avnd/Qt rotate() takes degrees; canvas takes radians.
    m_ctx.call<void>("rotate", a * (3.141592653589793 / 180.0));
  }
  void reset_transform()
  {
    m_ctx.call<void>("setTransform", 1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
  }

  void set_stroke_color(rgba_color c)
  {
    m_ctx.set("strokeStyle", rgba_string(c));
  }
  void set_fill_color(rgba_color c) { m_ctx.set("fillStyle", rgba_string(c)); }
  void set_stroke_width(double w) { m_ctx.set("lineWidth", w); }

  void set_font(std::string_view name)
  {
    m_font_family.assign(name.begin(), name.end());
    apply_font();
  }
  void set_font_size(double pts)
  {
    m_font_size = pts;
    apply_font();
  }
  void draw_text(double x, double y, std::string_view str)
  {
    const std::string s{str};
    m_ctx.call<void>("fillText", s, x, y);
  }

  // Self-contained line: strokes immediately (mirrors a Qt drawLine).
  void draw_line(double x1, double y1, double x2, double y2)
  {
    m_ctx.call<void>("beginPath");
    m_ctx.call<void>("moveTo", x1, y1);
    m_ctx.call<void>("lineTo", x2, y2);
    m_ctx.call<void>("stroke");
  }

  // Adds a rectangle subpath; fill()/stroke() commit it (avnd convention).
  void draw_rect(double x, double y, double w, double h)
  {
    m_ctx.call<void>("rect", x, y, w, h);
  }

  void draw_rounded_rect(double x, double y, double w, double h, double r)
  {
    if(has_round_rect())
    {
      m_ctx.call<void>("roundRect", x, y, w, h, r);
      return;
    }
    constexpr double half_pi = 3.141592653589793 / 2.0;
    const double rr = std::min(r, std::min(w, h) / 2.0);
    m_ctx.call<void>("moveTo", x + rr, y);
    m_ctx.call<void>("lineTo", x + w - rr, y);
    m_ctx.call<void>("arc", x + w - rr, y + rr, rr, -half_pi, 0.0);
    m_ctx.call<void>("lineTo", x + w, y + h - rr);
    m_ctx.call<void>("arc", x + w - rr, y + h - rr, rr, 0.0, half_pi);
    m_ctx.call<void>("lineTo", x + rr, y + h);
    m_ctx.call<void>("arc", x + rr, y + h - rr, rr, half_pi, 2.0 * half_pi);
    m_ctx.call<void>("lineTo", x, y + rr);
    m_ctx.call<void>("arc", x + rr, y + rr, rr, 2.0 * half_pi, 3.0 * half_pi);
    m_ctx.call<void>("closePath");
  }

  // (x,y,w,h) is a bounding box, like Qt addEllipse.
  void draw_ellipse(double x, double y, double w, double h)
  {
    constexpr double two_pi = 2.0 * 3.141592653589793;
    const double cx = x + w / 2.0;
    const double cy = y + h / 2.0;
    m_ctx.call<void>(
        "ellipse", cx, cy, w / 2.0, h / 2.0, 0.0, 0.0, two_pi);
  }

  void draw_circle(double cx, double cy, double r)
  {
    constexpr double two_pi = 2.0 * 3.141592653589793;
    m_ctx.call<void>("arc", cx, cy, r, 0.0, two_pi);
  }

  void draw_pixmap(double x, double y, std::string_view name)
  {
    // Best-effort lookup in a JS-side cache; absent -> graceful no-op.
    emscripten::val cache
        = emscripten::val::global("globalThis")["__avnd_images"];
    if(cache.isUndefined() || cache.isNull())
      return;
    const std::string key{name};
    emscripten::val img = cache[key];
    if(img.isUndefined() || img.isNull())
      return;
    m_ctx.call<void>("drawImage", img, x, y);
  }

  void draw_bytes(
      double x, double y, double w, double h, const unsigned char* bytes,
      int img_w, int img_h)
  {
    if(!bytes || img_w <= 0 || img_h <= 0)
      return;
    const std::size_t n = std::size_t(img_w) * std::size_t(img_h) * 4u;
    emscripten::val u8 = make_clamped_array(bytes, n);
    blit(u8, x, y, w, h, img_w, img_h);
  }

  void draw_bytes(
      double x, double y, double w, double h, const float* bytes, int img_w,
      int img_h)
  {
    if(!bytes || img_w <= 0 || img_h <= 0)
      return;
    const std::size_t n = std::size_t(img_w) * std::size_t(img_h) * 4u;
    // Convert RGBA32F (0..1) to clamped 8-bit.
    emscripten::val Uint8ClampedArray
        = emscripten::val::global("Uint8ClampedArray");
    emscripten::val u8 = Uint8ClampedArray.new_(int(n));
    for(std::size_t i = 0; i < n; ++i)
    {
      double v = double(bytes[i]) * 255.0;
      if(v < 0.0)
        v = 0.0;
      else if(v > 255.0)
        v = 255.0;
      u8.set(int(i), int(v + 0.5));
    }
    blit(u8, x, y, w, h, img_w, img_h);
  }

private:
  static std::string rgba_string(rgba_color c)
  {
    // CSS rgba(): alpha is 0..1.
    std::string s = "rgba(";
    s += std::to_string((int)c.r);
    s += ',';
    s += std::to_string((int)c.g);
    s += ',';
    s += std::to_string((int)c.b);
    s += ',';
    // Locale-independent alpha formatting.
    const double a = double(c.a) / 255.0;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.4f", a);
    s += buf;
    s += ')';
    return s;
  }

  void apply_font()
  {
    std::string f = std::to_string((int)(m_font_size + 0.5));
    f += "px ";
    f += (m_font_family.empty() ? "sans-serif" : m_font_family);
    m_ctx.set("font", f);
  }

  bool has_round_rect()
  {
    if(m_round_rect == 0)
      m_round_rect
          = (!m_ctx["roundRect"].isUndefined() && !m_ctx["roundRect"].isNull())
                ? 1
                : -1;
    return m_round_rect == 1;
  }

  static emscripten::val
  make_clamped_array(const unsigned char* bytes, std::size_t n)
  {
    // Copy into a fresh Uint8ClampedArray so ImageData owns its storage and the
    // wasm heap can move.
    emscripten::val Uint8ClampedArray
        = emscripten::val::global("Uint8ClampedArray");
    emscripten::val u8 = Uint8ClampedArray.new_(int(n));
    for(std::size_t i = 0; i < n; ++i)
      u8.set(int(i), int(bytes[i]));
    return u8;
  }

  // Construct an ImageData (img_w x img_h) wrapping the clamped byte array.
  // Prefer ctx.createImageData; fall back to the global ImageData constructor.
  emscripten::val make_image_data(emscripten::val u8, int img_w, int img_h)
  {
    if(!m_ctx["createImageData"].isUndefined()
       && !m_ctx["createImageData"].isNull())
    {
      emscripten::val imgData
          = m_ctx.call<emscripten::val>("createImageData", img_w, img_h);
      imgData["data"].call<void>("set", u8);
      return imgData;
    }
    emscripten::val ImageData = emscripten::val::global("ImageData");
    if(ImageData.isUndefined() || ImageData.isNull())
      return emscripten::val::null();
    return ImageData.new_(u8, img_w, img_h);
  }

  void blit(
      emscripten::val u8, double x, double y, double w, double h, int img_w,
      int img_h)
  {
    emscripten::val imgData = make_image_data(u8, img_w, img_h);
    if(imgData.isNull())
      return; // no ImageData available -> graceful no-op

    const bool scaled
        = (int(w + 0.5) != img_w) || (int(h + 0.5) != img_h) || (w <= 0)
          || (h <= 0);
    if(!scaled)
    {
      m_ctx.call<void>("putImageData", imgData, x, y);
      return;
    }

    // Temp canvas: putImageData at origin, then drawImage scaled.
    emscripten::val doc = emscripten::val::global("document");
    emscripten::val tmp;
    if(!doc.isUndefined() && !doc.isNull())
    {
      tmp = doc.call<emscripten::val>(
          "createElement", std::string{"canvas"});
    }
    else
    {
      // Worker / OffscreenCanvas environment.
      emscripten::val OffscreenCanvas
          = emscripten::val::global("OffscreenCanvas");
      if(OffscreenCanvas.isUndefined())
      {
        m_ctx.call<void>("putImageData", imgData, x, y);
        return;
      }
      tmp = OffscreenCanvas.new_(img_w, img_h);
    }
    tmp.set("width", img_w);
    tmp.set("height", img_h);
    emscripten::val tctx
        = tmp.call<emscripten::val>("getContext", std::string{"2d"});
    tctx.call<void>("putImageData", imgData, 0, 0);
    m_ctx.call<void>("drawImage", tmp, 0, 0, img_w, img_h, x, y, w, h);
  }

  emscripten::val m_ctx;
  std::string m_font_family{"sans-serif"};
  double m_font_size{10.0};
  int m_round_rect{0}; // 0 = unknown, 1 = yes, -1 = no
  bool m_dirty{false};
};

static_assert(avnd::painter<canvas_painter>);

#endif // __EMSCRIPTEN__

} // namespace wasm
