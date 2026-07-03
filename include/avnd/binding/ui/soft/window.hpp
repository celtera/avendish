#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Desktop shell for the soft UI runtime: a pugl child view embedded in the
 * host-provided parent window (the VST/CLAP editor contract), driven by a
 * pugl timer, blitting the runtime's RGBA framebuffer with plain platform
 * calls (no GPU context). Satisfies avnd::gui_windowed_ui so the plug-in
 * bindings stay framework-agnostic.
 *
 * Event flow: the host's UI-thread message pump (plus idle() calling
 * puglUpdate) dispatches pugl events; PUGL_TIMER runs the widget pass and
 * obscures the view when dirty; PUGL_EXPOSE rasterizes and blits.
 */

#include <avnd/binding/ui/soft/runtime.hpp>
#include <avnd/concepts/gui_window.hpp>

#include <pugl/pugl.h>
#include <pugl/stub.h>

#include <functional>

#if defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN 1
#endif
#if !defined(NOMINMAX)
#define NOMINMAX 1
#endif
#include <windows.h>
#elif defined(__linux__) || defined(__FreeBSD__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace avnd::soft_ui
{
template <typename T>
class window_editor
{
public:
  explicit window_editor(avnd::effect_container<T>& impl)
      : m_runtime{impl, system_fonts()}
  {
  }

  ~window_editor() { close(); }

  ui_runtime<T>& runtime() noexcept { return m_runtime; }

  std::pair<int, int> size() const
  {
    return {m_runtime.width(), m_runtime.height()};
  }

  static bool supports(avnd::gui_api api) noexcept
  {
#if defined(_WIN32)
    return api == avnd::gui_api::win32_hwnd;
#elif defined(__APPLE__)
    return api == avnd::gui_api::cocoa_nsview;
#else
    return api == avnd::gui_api::x11_window;
#endif
  }

  void open(avnd::gui_parent parent, avnd::gui_host host)
  {
    if(m_view)
      close();

    m_runtime.host = host;
    m_scale = parent.scale > 0. ? parent.scale : 1.;

    m_world = puglNewWorld(PUGL_MODULE, 0);
    m_view = puglNewView(m_world);
    puglSetHandle(m_view, this);
    puglSetEventFunc(m_view, &window_editor::on_event);
    puglSetBackend(m_view, puglStubBackend());
    puglSetViewHint(m_view, PUGL_RESIZABLE, PUGL_FALSE);
    puglSetSizeHint(
        m_view, PUGL_DEFAULT_SIZE, (PuglSpan)m_runtime.width(),
        (PuglSpan)m_runtime.height());
    if(parent.handle)
      puglSetParent(m_view, (PuglNativeView)parent.handle);

    if(puglRealize(m_view) != PUGL_SUCCESS)
    {
      puglFreeView(m_view);
      puglFreeWorld(m_world);
      m_view = nullptr;
      m_world = nullptr;
      return;
    }

    puglStartTimer(m_view, 0, 1. / 30.);
    puglShow(m_view, PUGL_SHOW_PASSIVE);
  }

  void close()
  {
    if(m_view)
    {
      puglStopTimer(m_view, 0);
      puglFreeView(m_view);
      m_view = nullptr;
    }
    if(m_world)
    {
      puglFreeWorld(m_world);
      m_world = nullptr;
    }
  }

  // Host UI-thread tick (VST2 effEditIdle, CLAP timer, ...). On Windows the
  // host's message pump also dispatches to the view directly; this makes
  // progress on hosts that only provide an idle callback.
  void idle()
  {
    if(m_world)
      puglUpdate(m_world, 0.);
  }

  void* native_view() const noexcept
  {
    return m_view ? (void*)puglGetNativeView(m_view) : nullptr;
  }

  // Called at the start of every UI frame (30 Hz pugl timer, UI thread).
  std::function<void()> on_frame;

private:
  static window_editor& self(PuglView* view)
  {
    return *static_cast<window_editor*>(puglGetHandle(view));
  }

  static PuglStatus on_event(PuglView* view, const PuglEvent* event)
  {
    auto& w = self(view);
    switch(event->type)
    {
      case PUGL_BUTTON_PRESS:
        w.m_runtime.pointer_button(event->button.x, event->button.y, true);
        break;
      case PUGL_BUTTON_RELEASE:
        w.m_runtime.pointer_button(event->button.x, event->button.y, false);
        break;
      case PUGL_MOTION:
        w.m_runtime.pointer_move(event->motion.x, event->motion.y);
        break;
      case PUGL_SCROLL:
        w.m_runtime.wheel(event->scroll.x, event->scroll.y, event->scroll.dy);
        break;
      case PUGL_TIMER:
        if(event->timer.id == 0)
        {
          // Binding hook: drain transport queues etc. on the UI thread even
          // when the host provides no idle callback of its own.
          if(w.on_frame)
            w.on_frame();
          if(w.m_runtime.tick())
            puglObscureView(view);
        }
        break;
      case PUGL_EXPOSE:
        w.render_and_blit(view);
        break;
      default:
        break;
    }
    return PUGL_SUCCESS;
  }

  void render_and_blit(PuglView* view)
  {
    const int w = m_runtime.width();
    const int h = m_runtime.height();
    m_pixels.resize(size_t(w) * h * 4);
    m_runtime.render({m_pixels.data(), w, h, w * 4});

#if defined(_WIN32)
    // RGBA -> BGRA for GDI
    m_blit.resize(m_pixels.size());
    for(size_t i = 0; i < m_pixels.size(); i += 4)
    {
      m_blit[i + 0] = m_pixels[i + 2];
      m_blit[i + 1] = m_pixels[i + 1];
      m_blit[i + 2] = m_pixels[i + 0];
      m_blit[i + 3] = m_pixels[i + 3];
    }

    const HWND hwnd = (HWND)puglGetNativeView(view);
    if(const HDC dc = GetDC(hwnd))
    {
      BITMAPINFO bmi{};
      bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth = w;
      bmi.bmiHeader.biHeight = -h; // top-down
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;
      StretchDIBits(
          dc, 0, 0, w, h, 0, 0, w, h, m_blit.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
      ReleaseDC(hwnd, dc);
    }
#elif defined(__linux__) || defined(__FreeBSD__)
    // X11: BGRX image put straight onto the window.
    m_blit.resize(m_pixels.size());
    for(size_t i = 0; i < m_pixels.size(); i += 4)
    {
      m_blit[i + 0] = m_pixels[i + 2];
      m_blit[i + 1] = m_pixels[i + 1];
      m_blit[i + 2] = m_pixels[i + 0];
      m_blit[i + 3] = 0xFF;
    }
    if(auto* dpy = (Display*)puglGetNativeWorld(puglGetWorld(view)))
    {
      const auto win = (::Window)puglGetNativeView(view);
      const int screen = DefaultScreen(dpy);
      XImage img{};
      img.width = w;
      img.height = h;
      img.format = ZPixmap;
      img.data = (char*)m_blit.data();
      img.byte_order = LSBFirst;
      img.bitmap_unit = 32;
      img.bitmap_bit_order = LSBFirst;
      img.bitmap_pad = 32;
      img.depth = 24;
      img.bytes_per_line = w * 4;
      img.bits_per_pixel = 32;
      XInitImage(&img);
      const GC gc = XCreateGC(dpy, win, 0, nullptr);
      XPutImage(dpy, win, gc, &img, 0, 0, 0, 0, w, h);
      XFreeGC(dpy, gc);
    }
#else
    // macOS blit lands with the Cocoa pass (CUSTOM_UI_PLAN.md phase 3).
#endif
  }

  ui_runtime<T> m_runtime;
  PuglWorld* m_world{};
  PuglView* m_view{};
  std::vector<unsigned char> m_pixels;
  std::vector<unsigned char> m_blit;
  double m_scale{1.};
};
}
