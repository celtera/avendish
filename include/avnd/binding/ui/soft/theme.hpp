#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Instrument-style dark theme for the soft editor: neutral dark surfaces,
 * halp's orange accent for value cursors (sliders, knobs, progress), mapped
 * onto Nuklear's color table. The window background follows the layout's
 * halp_meta(background, halp::colors::...) when present.
 */

#include <avnd/binding/ui/soft/nk_config.hpp>

namespace avnd::soft_ui
{
// halp::colors::background_* shades
inline nk_color background_shade(int degree) noexcept
{
  switch(degree)
  {
    case 0: return nk_rgb(15, 15, 18);  // background_darker
    case 1: return nk_rgb(22, 22, 26);  // background_dark
    default:
    case 2: return nk_rgb(30, 31, 36);  // background_mid
    case 3: return nk_rgb(42, 43, 50);  // background_light
    case 4: return nk_rgb(56, 58, 66);  // background_lighter
  }
}

inline void apply_instrument_theme(nk_context* ctx, nk_color window_bg)
{
  const nk_color text = nk_rgb(222, 222, 226);
  const nk_color surface = nk_rgb(45, 46, 52);
  const nk_color surface_hover = nk_rgb(58, 60, 68);
  const nk_color surface_active = nk_rgb(70, 72, 82);
  const nk_color track = nk_rgb(26, 27, 31);
  const nk_color border = nk_rgb(12, 12, 14);
  const nk_color accent = nk_rgb(255, 176, 30);
  const nk_color accent_hover = nk_rgb(255, 200, 80);
  const nk_color accent_active = nk_rgb(255, 160, 0);

  nk_color table[NK_COLOR_COUNT];
  table[NK_COLOR_TEXT] = text;
  table[NK_COLOR_WINDOW] = window_bg;
  table[NK_COLOR_HEADER] = track;
  table[NK_COLOR_BORDER] = border;
  table[NK_COLOR_BUTTON] = surface;
  table[NK_COLOR_BUTTON_HOVER] = surface_hover;
  table[NK_COLOR_BUTTON_ACTIVE] = accent_active;
  table[NK_COLOR_TOGGLE] = track;
  table[NK_COLOR_TOGGLE_HOVER] = surface_hover;
  table[NK_COLOR_TOGGLE_CURSOR] = accent;
  table[NK_COLOR_SELECT] = track;
  table[NK_COLOR_SELECT_ACTIVE] = accent_active;
  table[NK_COLOR_SLIDER] = track;
  table[NK_COLOR_SLIDER_CURSOR] = accent;
  table[NK_COLOR_SLIDER_CURSOR_HOVER] = accent_hover;
  table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = accent_active;
  table[NK_COLOR_PROPERTY] = surface;
  table[NK_COLOR_EDIT] = track;
  table[NK_COLOR_EDIT_CURSOR] = text;
  table[NK_COLOR_COMBO] = surface;
  table[NK_COLOR_CHART] = track;
  table[NK_COLOR_CHART_COLOR] = accent;
  table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = accent_hover;
  table[NK_COLOR_SCROLLBAR] = window_bg;
  table[NK_COLOR_SCROLLBAR_CURSOR] = surface;
  table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = surface_hover;
  table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = surface_active;
  table[NK_COLOR_TAB_HEADER] = track;
  table[NK_COLOR_KNOB] = track;
  table[NK_COLOR_KNOB_CURSOR] = accent;
  table[NK_COLOR_KNOB_CURSOR_HOVER] = accent_hover;
  table[NK_COLOR_KNOB_CURSOR_ACTIVE] = accent_active;
  nk_style_from_table(ctx, table);

  // A bit more air than the defaults
  ctx->style.window.padding = nk_vec2(8, 8);
  ctx->style.window.spacing = nk_vec2(6, 6);
  ctx->style.slider.cursor_size = nk_vec2(14, 14);
}
}
