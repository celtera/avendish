#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <string>

namespace halp
{
/**
 * A sound file drawn by the host, for custom widgets.
 *
 * A painter that supports it draws the file with
 * `ctx.draw_waveform(view, x, y, w, h)`, wherever the widget's own painting
 * puts it. Hosts that have their own waveform renderer use it, so a widget
 * does not have to decode the file nor send its envelope around. Check that
 * the painter has it before calling it:
 *
 *   if constexpr(requires { ctx.draw_waveform(view, 0., 0., 1., 1.); })
 *     ctx.draw_waveform(view, 0., 0., width(), height());
 */
struct waveform_view
{
  //! Set by the widget: the file to draw, and the part of it that is shown,
  //! as fractions of its length.
  std::string file;
  double begin{0.};
  double end{1.};

  //! Set by the host when it draws: whether there is a waveform to show yet,
  //! and what it knows of the file once it is decoded.
  bool ready{};
  double duration{}; // seconds
  int channels{};
};
}
