#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Replays a Nuklear command queue onto a canvas_ity canvas.
 *
 * This replaces Nuklear's demo rawfb rasterizer (planned in
 * CUSTOM_UI_PLAN.md §6.1): replaying the command list through the same
 * software vector canvas that backs custom paint() widgets gives one
 * antialiased pipeline, one font path, and zero dependence on Nuklear's
 * demo headers.
 */

#include <avnd/binding/ui/soft/fonts.hpp>
#include <avnd/binding/ui/soft/nk_config.hpp>

#include <canvas_ity.hpp>

#include <string>

namespace avnd::soft_ui
{

class nk_canvas_renderer
{
public:
  // A tiny canvas whose only job is to carry font state for measure_text();
  // it doubles as the nk_user_font width callback.
  struct text_measure
  {
    canvas_ity::canvas canvas{1, 1};
    const font_registry* fonts{};
    std::string scratch;
    float applied_height{-1.f};

    void apply(float height)
    {
      if(applied_height == height)
        return;
      if(auto* f = fonts ? fonts->default_font() : nullptr)
      {
        canvas.set_font(f->bytes.data(), (int)f->bytes.size(), height);
        applied_height = height;
      }
    }

    static float
    width_cb(nk_handle handle, float height, const char* text, int len)
    {
      auto& self = *static_cast<text_measure*>(handle.ptr);
      self.apply(height);
      self.scratch.assign(text, text + len);
      return self.canvas.measure_text(self.scratch.c_str());
    }
  };

  explicit nk_canvas_renderer(const font_registry& fonts)
  {
    m_measure.fonts = &fonts;
  }

  // The font to hand to nk_init_*; height in pixels.
  nk_user_font make_font(float height)
  {
    nk_user_font f{};
    f.userdata = nk_handle_ptr(&m_measure);
    f.height = height;
    f.width = &text_measure::width_cb;
    return f;
  }

  void render(nk_context* ctx, canvas_ity::canvas& c)
  {
    // Text rasterization needs the actual TTF; positioned from the glyph
    // box top like Nuklear expects.
    c.text_baseline = canvas_ity::top;

    const nk_command* cmd;
    bool clipped = false;
    float applied_font = -1.f;
    nk_foreach(cmd, ctx)
    {
      switch(cmd->type)
      {
        case NK_COMMAND_NOP:
          break;

        case NK_COMMAND_SCISSOR: {
          const auto& s = *reinterpret_cast<const nk_command_scissor*>(cmd);
          if(clipped)
            c.restore();
          c.save();
          clipped = true;
          c.begin_path();
          c.rectangle(s.x, s.y, s.w, s.h);
          c.clip();
          c.begin_path();
          // restore() pops font & baseline state too
          applied_font = -1.f;
          c.text_baseline = canvas_ity::top;
          break;
        }

        case NK_COMMAND_LINE: {
          const auto& s = *reinterpret_cast<const nk_command_line*>(cmd);
          stroke_color(c, s.color, s.line_thickness);
          c.begin_path();
          c.move_to(s.begin.x, s.begin.y);
          c.line_to(s.end.x, s.end.y);
          c.stroke();
          break;
        }

        case NK_COMMAND_CURVE: {
          const auto& s = *reinterpret_cast<const nk_command_curve*>(cmd);
          stroke_color(c, s.color, s.line_thickness);
          c.begin_path();
          c.move_to(s.begin.x, s.begin.y);
          c.bezier_curve_to(
              s.ctrl[0].x, s.ctrl[0].y, s.ctrl[1].x, s.ctrl[1].y, s.end.x, s.end.y);
          c.stroke();
          break;
        }

        case NK_COMMAND_RECT: {
          const auto& s = *reinterpret_cast<const nk_command_rect*>(cmd);
          stroke_color(c, s.color, s.line_thickness);
          c.begin_path();
          rounded_rect(c, s.x, s.y, s.w, s.h, s.rounding);
          c.stroke();
          break;
        }

        case NK_COMMAND_RECT_FILLED: {
          const auto& s = *reinterpret_cast<const nk_command_rect_filled*>(cmd);
          fill_color(c, s.color);
          c.begin_path();
          rounded_rect(c, s.x, s.y, s.w, s.h, s.rounding);
          c.fill();
          break;
        }

        case NK_COMMAND_RECT_MULTI_COLOR: {
          const auto& s = *reinterpret_cast<const nk_command_rect_multi_color*>(cmd);
          // Approximated with a vertical gradient (used by e.g. color pickers).
          c.set_linear_gradient(
              canvas_ity::fill_style, (float)s.x, (float)s.y, (float)s.x,
              (float)(s.y + s.h));
          c.add_color_stop(
              canvas_ity::fill_style, 0.f, s.top.r / 255.f, s.top.g / 255.f,
              s.top.b / 255.f, s.top.a / 255.f);
          c.add_color_stop(
              canvas_ity::fill_style, 1.f, s.bottom.r / 255.f, s.bottom.g / 255.f,
              s.bottom.b / 255.f, s.bottom.a / 255.f);
          c.begin_path();
          c.rectangle(s.x, s.y, s.w, s.h);
          c.fill();
          break;
        }

        case NK_COMMAND_CIRCLE: {
          const auto& s = *reinterpret_cast<const nk_command_circle*>(cmd);
          stroke_color(c, s.color, s.line_thickness);
          c.begin_path();
          ellipse(c, s.x, s.y, s.w, s.h);
          c.stroke();
          break;
        }

        case NK_COMMAND_CIRCLE_FILLED: {
          const auto& s = *reinterpret_cast<const nk_command_circle_filled*>(cmd);
          fill_color(c, s.color);
          c.begin_path();
          ellipse(c, s.x, s.y, s.w, s.h);
          c.fill();
          break;
        }

        case NK_COMMAND_ARC: {
          const auto& s = *reinterpret_cast<const nk_command_arc*>(cmd);
          stroke_color(c, s.color, s.line_thickness);
          c.begin_path();
          c.arc(s.cx, s.cy, s.r, s.a[0], s.a[1]);
          c.stroke();
          break;
        }

        case NK_COMMAND_ARC_FILLED: {
          const auto& s = *reinterpret_cast<const nk_command_arc_filled*>(cmd);
          fill_color(c, s.color);
          c.begin_path();
          c.move_to(s.cx, s.cy);
          c.arc(s.cx, s.cy, s.r, s.a[0], s.a[1]);
          c.close_path();
          c.fill();
          break;
        }

        case NK_COMMAND_TRIANGLE: {
          const auto& s = *reinterpret_cast<const nk_command_triangle*>(cmd);
          stroke_color(c, s.color, s.line_thickness);
          c.begin_path();
          c.move_to(s.a.x, s.a.y);
          c.line_to(s.b.x, s.b.y);
          c.line_to(s.c.x, s.c.y);
          c.close_path();
          c.stroke();
          break;
        }

        case NK_COMMAND_TRIANGLE_FILLED: {
          const auto& s = *reinterpret_cast<const nk_command_triangle_filled*>(cmd);
          fill_color(c, s.color);
          c.begin_path();
          c.move_to(s.a.x, s.a.y);
          c.line_to(s.b.x, s.b.y);
          c.line_to(s.c.x, s.c.y);
          c.close_path();
          c.fill();
          break;
        }

        case NK_COMMAND_POLYGON:
        case NK_COMMAND_POLYLINE: {
          const auto& s = *reinterpret_cast<const nk_command_polygon*>(cmd);
          stroke_color(c, s.color, s.line_thickness);
          c.begin_path();
          c.move_to(s.points[0].x, s.points[0].y);
          for(int i = 1; i < s.point_count; i++)
            c.line_to(s.points[i].x, s.points[i].y);
          if(cmd->type == NK_COMMAND_POLYGON)
            c.close_path();
          c.stroke();
          break;
        }

        case NK_COMMAND_POLYGON_FILLED: {
          const auto& s = *reinterpret_cast<const nk_command_polygon_filled*>(cmd);
          fill_color(c, s.color);
          c.begin_path();
          c.move_to(s.points[0].x, s.points[0].y);
          for(int i = 1; i < s.point_count; i++)
            c.line_to(s.points[i].x, s.points[i].y);
          c.close_path();
          c.fill();
          break;
        }

        case NK_COMMAND_TEXT: {
          const auto& s = *reinterpret_cast<const nk_command_text*>(cmd);
          if(auto* f = m_measure.fonts->default_font(); f && s.length > 0)
          {
            const float h = s.height > 0 ? s.height : s.font->height;
            if(applied_font != h)
            {
              c.set_font(f->bytes.data(), (int)f->bytes.size(), h);
              applied_font = h;
            }
            fill_color(c, s.foreground);
            m_text.assign(s.string, s.string + s.length);
            c.fill_text(m_text.c_str(), s.x, s.y);
          }
          break;
        }

        case NK_COMMAND_IMAGE:
          // Image atlas support comes with the resource story.
          break;

        case NK_COMMAND_CUSTOM:
          break;
      }
    }
    if(clipped)
      c.restore();
  }

private:
  static void fill_color(canvas_ity::canvas& c, nk_color col)
  {
    c.set_color(
        canvas_ity::fill_style, col.r / 255.f, col.g / 255.f, col.b / 255.f,
        col.a / 255.f);
  }

  static void stroke_color(canvas_ity::canvas& c, nk_color col, float thickness)
  {
    c.set_color(
        canvas_ity::stroke_style, col.r / 255.f, col.g / 255.f, col.b / 255.f,
        col.a / 255.f);
    c.set_line_width(thickness > 0.f ? thickness : 1.f);
  }

  static void
  rounded_rect(canvas_ity::canvas& c, float x, float y, float w, float h, float r)
  {
    if(r <= 0.f)
    {
      c.rectangle(x, y, w, h);
      return;
    }
    if(r > w / 2.f)
      r = w / 2.f;
    if(r > h / 2.f)
      r = h / 2.f;
    c.move_to(x + r, y);
    c.arc_to(x + w, y, x + w, y + h, r);
    c.arc_to(x + w, y + h, x, y + h, r);
    c.arc_to(x, y + h, x, y, r);
    c.arc_to(x, y, x + w, y, r);
    c.close_path();
  }

  // Nuklear circles come as bounding boxes; canvas_ity arcs are circular,
  // so draw a unit circle under a temporary transform.
  static void ellipse(canvas_ity::canvas& c, float x, float y, float w, float h)
  {
    if(w <= 0.f || h <= 0.f)
      return;
    c.save();
    c.translate(x + w / 2.f, y + h / 2.f);
    c.scale(w / 2.f, h / 2.f);
    c.move_to(1.f, 0.f);
    c.arc(0.f, 0.f, 1.f, 0.f, 6.2831853f);
    c.restore();
  }

  text_measure m_measure;
  std::string m_text;
};
}
