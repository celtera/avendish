#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <halp/custom_widgets.hpp>
#include <halp/layout.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace halp
{
// An ADSR envelope drawn from its four parameters and edited by dragging its
// points: the peak sets the attack, the knee the decay (sideways) and the
// sustain level (up and down), the end the release.
//
//   halp::custom_multi_control<halp::envelope_editor,
//       &ins::attack, &ins::decay, &ins::sustain, &ins::release> env;
//
// basic_envelope_editor<W, H> draws it at another size.
template <int Width = 96, int Height = 32>
struct basic_envelope_editor
{
  static constexpr double w = Width;
  static constexpr double h = Height;
  static constexpr double width() { return w; }
  static constexpr double height() { return h; }

  std::array<double, 4> values{0.1, 0.3, 0.7, 0.4};
  multi_transaction transaction;

  // ---- geometry: A, D and R get at most a quarter of the width each, the
  // sustain plateau keeps the rest. Times are shown on a square-root scale,
  // so that short ones - the usual case - stay wide enough to grab.
  static constexpr double pad = 3.;
  static constexpr double segment = (w - 2. * pad) / 4.;

  struct points
  {
    double x0, y0, xa, ya, xd, yd, xs, xr, yr;
  };

  points geometry() const noexcept
  {
    auto t = [](double v) { return std::sqrt(std::clamp(v, 0., 1.)); };
    const double top = pad;
    const double bottom = height() - pad;
    const double sus_y = bottom - (bottom - top) * std::clamp(values[2], 0., 1.);
    points p{};
    p.x0 = pad;
    p.y0 = bottom;
    p.xa = p.x0 + segment * t(values[0]);
    p.ya = top;
    p.xd = p.xa + segment * t(values[1]);
    p.yd = sus_y;
    p.xs = width() - pad - segment;
    p.xr = p.xs + segment * t(values[3]);
    p.yr = bottom;
    return p;
  }

  // ---- painting
  template <typename Ctx>
  static auto colors(Ctx& ctx)
  {
    struct palette
    {
      std::array<unsigned char, 4> well{24, 25, 25, 255}, line{224, 176, 30, 255},
          fill{224, 176, 30, 60}, handle{255, 255, 255, 220};
    } p;
    if constexpr(requires { ctx.to_rgba(halp::colors::light); })
    {
      auto get = [&](halp::colors c, unsigned char a = 255) {
        auto v = ctx.to_rgba(c);
        return std::array<unsigned char, 4>{v.r, v.g, v.b, a};
      };
      p.well = get(halp::colors::background_dark);
      p.line = get(halp::colors::runtime_value_mid);
      p.fill = get(halp::colors::runtime_value_mid, 60);
      p.handle = get(halp::colors::lighter);
    }
    return p;
  }

  void paint(auto ctx)
  {
    const auto pal = colors(ctx);
    const auto p = geometry();

    ctx.begin_path();
    ctx.set_fill_color({pal.well[0], pal.well[1], pal.well[2], pal.well[3]});
    ctx.draw_rounded_rect(0., 0., width(), height(), 2.);
    ctx.fill();

    auto shape = [&] {
      ctx.move_to(p.x0, p.y0);
      ctx.line_to(p.xa, p.ya);
      ctx.line_to(p.xd, p.yd);
      ctx.line_to(p.xs, p.yd);
      ctx.line_to(p.xr, p.yr);
    };

    ctx.begin_path();
    shape();
    ctx.close_path();
    ctx.set_fill_color({pal.fill[0], pal.fill[1], pal.fill[2], pal.fill[3]});
    ctx.fill();

    ctx.begin_path();
    shape();
    ctx.set_stroke_width(1.5);
    ctx.set_stroke_color({pal.line[0], pal.line[1], pal.line[2], pal.line[3]});
    ctx.stroke();

    ctx.begin_path();
    ctx.set_fill_color({pal.handle[0], pal.handle[1], pal.handle[2], pal.handle[3]});
    ctx.draw_circle(p.xa, p.ya, dragging == attack_peak ? 3. : 2.);
    ctx.draw_circle(p.xd, p.yd, dragging == decay_knee ? 3. : 2.);
    ctx.draw_circle(p.xr, p.yr, dragging == release_end ? 3. : 2.);
    ctx.fill();
  }

  // ---- interaction
  enum grabbed_point
  {
    none = -1,
    attack_peak,
    decay_knee,
    release_end
  };
  grabbed_point dragging{none};

  // Drags are relative: a full sweep of a time takes this many pixels, far
  // more than the drawing gives it, for fine control in a small widget.
  static constexpr double time_span = 120.;
  static constexpr double level_span = 60.;
  double press_x{}, press_y{};
  std::array<double, 4> press_values{};

  grabbed_point handle_at(double x, double y) const noexcept
  {
    const auto p = geometry();
    grabbed_point best = none;
    double best_d = 8. * 8.;
    auto consider = [&](grabbed_point which, double px, double py) {
      const double d = (x - px) * (x - px) + (y - py) * (y - py);
      if(d < best_d)
      {
        best_d = d;
        best = which;
      }
    };
    consider(attack_peak, p.xa, p.ya);
    consider(decay_knee, p.xd, p.yd);
    consider(release_end, p.xr, p.yr);
    return best;
  }

  bool mouse_press(double x, double y)
  {
    dragging = handle_at(x, y);
    if(dragging == none)
      return false;
    press_x = x;
    press_y = y;
    press_values = values;
    if(transaction.start)
      transaction.start();
    return true;
  }

  bool mouse_move(double x, double y)
  {
    if(dragging == none)
      return false;
    // Times move on the same square-root scale they are drawn with
    auto time = [&](int index) {
      const double t = std::sqrt(press_values[index]) + (x - press_x) / time_span;
      return std::pow(std::clamp(t, 0., 1.), 2.);
    };
    auto set = [&](int index, double v) {
      values[index] = std::clamp(v, 0., 1.);
      if(transaction.update)
        transaction.update(index, values[index]);
    };
    switch(dragging)
    {
      case attack_peak:
        set(0, time(0));
        break;
      case decay_knee:
        set(1, time(1));
        set(2, press_values[2] - (y - press_y) / level_span);
        break;
      case release_end:
        set(3, time(3));
        break;
      default:
        break;
    }
    return true;
  }

  bool mouse_release(double, double)
  {
    if(dragging != none && transaction.commit)
      transaction.commit();
    dragging = none;
    return true;
  }
};

// Thumbnail size, e.g. in a table cell
using envelope_editor = basic_envelope_editor<>;
}
