/* SPDX-License-Identifier: GPL-3.0-or-later */

// Tier C editor -- Linux implementation. A raw Xlib window, living behind the
// pimpl forward-declared in CustomUiWindow.hpp. See that header for the
// contract; the Windows sibling is CustomUiWindow.win32.cpp.

#include "CustomUiWindow.hpp"

#if defined(__linux__) && __has_include(<X11/Xlib.h>)

#include <algorithm>
#include <cstdio>
#include <cstring>

// Xlib last, so its Bool/Status/None/... macros never leak into the headers
// above; this TU uses none of the colliding identifiers after this point.
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace examples
{
// The whole framework side of the window: it owns its own display connection,
// like a toolkit would; the host only ever hands over the parent window id.
struct CustomUiWindow::ui::window::impl
{
  explicit impl(window& owner, avnd::effect_container<CustomUiWindow>& fx)
      : owner{owner}
      , fx{fx}
  {
  }

  ~impl() { close(); }

  void open(avnd::gui_parent parent, avnd::gui_host h)
  {
    host = h;

    dpy = XOpenDisplay(nullptr);
    if(!dpy)
      return;
    const int screen = DefaultScreen(dpy);
    win = XCreateSimpleWindow(
        dpy, (::Window)(uintptr_t)parent.handle, 0, 0, window::width,
        window::height, 0, BlackPixel(dpy, screen), rgb(30, 30, 34));
    XSelectInput(
        dpy, win,
        ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    gc = XCreateGC(dpy, win, 0, nullptr);
    XMapWindow(dpy, win);
    XFlush(dpy);
  }

  void close()
  {
    if(dpy)
    {
      if(gc)
        XFreeGC(dpy, gc);
      if(win)
        XDestroyWindow(dpy, win);
      XCloseDisplay(dpy);
      dpy = nullptr;
      win = 0;
      gc = nullptr;
    }
  }

  // Host idle tick (effEditIdle, clap.timer-support, VST3 IRunLoop): this is
  // the author window's event loop *and* its repaint timer, so host automation
  // shows up without any callback plumbing.
  void idle()
  {
    if(!dpy)
      return;
    while(XPending(dpy))
    {
      XEvent ev;
      XNextEvent(dpy, &ev);
      switch(ev.type)
      {
        case ButtonPress:
          if(ev.xbutton.button == Button1)
          {
            dragging = true;
            host.begin_edit(host.ctx, window::gain_param);
            set_gain_from(ev.xbutton.x);
          }
          break;
        case MotionNotify:
          if(dragging)
            set_gain_from(ev.xmotion.x);
          break;
        case ButtonRelease:
          if(ev.xbutton.button == Button1 && dragging)
          {
            dragging = false;
            host.end_edit(host.ctx, window::gain_param);
            if(owner.send_message)
              owner.send_message(ui_to_processor{.value = gain()});
          }
          break;
        default:
          break;
      }
    }
    paint();
  }

private:
  static unsigned long rgb(int r, int g, int b)
  {
    // Direct pixel for the ubiquitous 24-bit TrueColor visual: an example is
    // allowed this shortcut, a real toolkit would XAllocColor.
    return ((unsigned long)r << 16) | ((unsigned long)g << 8)
           | (unsigned long)b;
  }

  float gain() const { return fx.effect.inputs.gain.value; }

  void set_gain_from(int x)
  {
    const float v = std::clamp(float(x) / float(window::width), 0.f, 1.f);
    // Write the model first, then tell the host: the bindings sample the
    // current value when broadcasting the automation point.
    fx.effect.inputs.gain.value = v;
    host.perform_edit(host.ctx, window::gain_param, v);
  }

  void paint()
  {
    XSetForeground(dpy, gc, rgb(30, 30, 34));
    XFillRectangle(dpy, win, gc, 0, 0, window::width, window::height);

    XSetForeground(dpy, gc, rgb(255, 176, 30));
    XFillRectangle(
        dpy, win, gc, 16, 32, (unsigned)((window::width - 32) * gain()),
        window::height - 64);

    char label[64];
    std::snprintf(label, sizeof(label), "gain: %.2f", gain());
    XSetForeground(dpy, gc, rgb(220, 220, 220));
    XDrawString(dpy, win, gc, 16, 20, label, (int)strlen(label));
    XFlush(dpy);
  }

  window& owner;
  avnd::effect_container<CustomUiWindow>& fx;
  avnd::gui_host host{};
  Display* dpy{};
  ::Window win{};
  GC gc{};
  bool dragging{};
};

CustomUiWindow::ui::window::window(avnd::effect_container<CustomUiWindow>& fx)
    : self{std::make_unique<impl>(*this, fx)}
{
}

CustomUiWindow::ui::window::~window() = default;

void CustomUiWindow::ui::window::open(avnd::gui_parent parent, avnd::gui_host h)
{
  self->open(parent, h);
}

void CustomUiWindow::ui::window::close()
{
  self->close();
}

void CustomUiWindow::ui::window::idle()
{
  self->idle();
}
}

#endif
