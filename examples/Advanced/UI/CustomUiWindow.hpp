#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Tier C custom-UI example (CUSTOM_UI_PLAN.md §5): instead of the
 * declarative `struct ui` rendered by the reference soft editor, the plug-in
 * ships its own editor implementing the avnd::gui_windowed_ui shape:
 *
 *     struct ui {
 *       struct window {
 *         void open(avnd::gui_parent, avnd::gui_host);
 *         void close();
 *         void idle();
 *         std::pair<int, int> size() const;
 *       };
 *     };
 *
 * Any UI stack works behind this seam -- Dear ImGui, Qt, JUCE, ... This
 * example deliberately uses no library at all: a raw Win32 child window
 * painted with GDI, so the contract itself is the whole story:
 *
 *  - open() receives the host's parent window handle and creates whatever
 *    the framework needs inside it;
 *  - parameter *reads* poll the effect model (live processor instance in
 *    CLAP/VST2, controller-side model in VST3), so host automation shows up
 *    without any extra plumbing;
 *  - parameter *writes* set the field, then drive the automation-gesture
 *    hooks on avnd::gui_host (flat parameter index, [0; 1] normalized) --
 *    the bindings translate to CLAP gesture events / audioMasterAutomate /
 *    IComponentHandler;
 *  - ticks come from the host (clap.timer-support, effEditIdle) and from
 *    the host's message pump for frameworks with their own timers, like
 *    the SetTimer used here.
 *
 * Two hand-rolled implementations of the same contract: raw Win32 + GDI on
 * Windows, raw Xlib on Linux (when the X11 headers are present). On other
 * platforms the plug-in simply has no editor (the concept is not satisfied,
 * bindings skip it).
 */

#include <avnd/concepts/gui_window.hpp>
#include <avnd/wrappers/effect_container.hpp>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#if defined(_WIN32)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN 1
#endif
#if !defined(NOMINMAX)
#define NOMINMAX 1
#endif
#include <windows.h>
// windows.h macro hygiene: these collide with enumerator names in binding
// headers included after this file (e.g. vintage.hpp's key enums).
#undef DELETE
#undef ALTERNATE
#undef WINDING
#elif defined(__linux__) && __has_include(<X11/Xlib.h>)
// Pull Xutil.h too (it needs the Bool/Status macros): later headers in the
// same TU that want it (the soft UI blit) will then no-op on their include.
#include <X11/Xlib.h>
#include <X11/Xutil.h>
// Xlib macro hygiene: this header is included in the middle of plug-in TUs,
// so drop the macros that collide with everything (keep the event type and
// mask constants, they are namespaced enough in practice).
#undef None
#undef Bool
#undef True
#undef False
#undef Status
#undef Always
#undef Success
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <utility>

namespace examples
{
struct CustomUiWindow
{
  halp_meta(name, "Custom UI window")
  halp_meta(c_name, "avnd_custom_ui_window")
  halp_meta(category, "Demo")
  halp_meta(author, "Avendish")
  halp_meta(description, "Gain with a hand-rolled (raw Win32) editor")
  halp_meta(uuid, "aef14f10-fb50-45e9-9dc6-c40c5e5fe790")

  struct ins
  {
    halp::knob_f32<"Gain", halp::range{.min = 0., .max = 1., .init = 0.5}> gain;
    halp::dynamic_audio_bus<"In", float> audio;
  } inputs;

  struct outs
  {
    halp::dynamic_audio_bus<"Out", float> audio;
  } outputs;

  void operator()(int frames)
  {
    for(int c = 0; c < outputs.audio.channels; c++)
      for(int i = 0; i < frames; i++)
        outputs.audio.samples[c][i] = inputs.audio.samples[c][i] * inputs.gain.value;
  }

  // Message bus for Tier C: the window itself is the UI-side endpoint --
  // the bindings wire window.send_message and call window.process_message.
  struct ui_to_processor
  {
    float value;
  };
  struct processor_to_ui
  {
    float applied;
  };

  void process_message(const ui_to_processor& msg)
  {
    // Deliberately not the same as the direct edit (msg.value / 2) so the
    // bus path is observable in tests.
    inputs.gain.value = msg.value * 0.5f;
    if(send_message)
      send_message(processor_to_ui{.applied = inputs.gain.value});
  }
  std::function<void(processor_to_ui)> send_message;

#if defined(_WIN32)
  struct ui
  {
    struct window
    {
      static constexpr int width = 360, height = 120;
      static constexpr int gain_param = 0; // flat index in parameter order

      // Bindings pass the model when the constructor accepts it
      // (avnd::make_ui_editor); in CLAP/VST2 this is the live processor
      // instance, in VST3 the controller-side model.
      explicit window(avnd::effect_container<CustomUiWindow>& fx)
          : fx{fx}
      {
      }

      ~window() { close(); }

      void open(avnd::gui_parent parent, avnd::gui_host h)
      {
        host = h;

        WNDCLASSW wc{};
        wc.lpfnWndProc = &window::wndproc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"avnd_custom_ui_window_example";
        RegisterClassW(&wc); // idempotent: re-registration just fails

        hwnd = CreateWindowExW(
            0, wc.lpszClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, width, height,
            (HWND)parent.handle, nullptr, wc.hInstance, this);

        // Repaint tick, dispatched by the host's message pump: host
        // automation moves the bar without any callback plumbing.
        SetTimer(hwnd, 1, 33, nullptr);
      }

      void close()
      {
        if(hwnd)
        {
          KillTimer(hwnd, 1);
          DestroyWindow(hwnd);
          hwnd = nullptr;
        }
      }

      // Host idle tick (effEditIdle, clap.timer-support): nothing to do,
      // WM_TIMER already drives us through the message pump.
      void idle() { }

      std::pair<int, int> size() const { return {width, height}; }

      // ---- Message bus endpoints (wired by the bindings) ----
      // UI thread -> processor; called on mouse release below.
      std::function<void(ui_to_processor)> send_message;
      // processor -> UI thread
      void process_message(processor_to_ui msg) { feedback = msg.applied; }
      float feedback{-1.f};

    private:
      float gain() const { return fx.effect.inputs.gain.value; }

      void set_gain_from(int x)
      {
        const float v = std::clamp(float(x) / float(width), 0.f, 1.f);
        // Write the model first, then tell the host: the bindings sample
        // the current value when broadcasting the automation point.
        fx.effect.inputs.gain.value = v;
        host.perform_edit(host.ctx, gain_param, v);
      }

      void paint(HDC dc)
      {
        RECT rc{0, 0, width, height};
        const HBRUSH bg = CreateSolidBrush(RGB(30, 30, 34));
        FillRect(dc, &rc, bg);
        DeleteObject(bg);

        RECT bar{16, 32, 16 + (int)((width - 32) * gain()), height - 32};
        const HBRUSH fill = CreateSolidBrush(RGB(255, 176, 30));
        FillRect(dc, &bar, fill);
        DeleteObject(fill);

        char label[64];
        std::snprintf(label, sizeof(label), "gain: %.2f", gain());
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(220, 220, 220));
        TextOutA(dc, 16, 8, label, (int)strlen(label));
      }

      static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
      {
        if(msg == WM_NCCREATE)
        {
          auto* cs = (CREATESTRUCTW*)lp;
          SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
          return DefWindowProcW(h, msg, wp, lp);
        }
        auto* self = (window*)GetWindowLongPtrW(h, GWLP_USERDATA);
        if(!self)
          return DefWindowProcW(h, msg, wp, lp);

        switch(msg)
        {
          case WM_LBUTTONDOWN:
            SetCapture(h);
            self->dragging = true;
            self->host.begin_edit(self->host.ctx, gain_param);
            self->set_gain_from((int)(short)LOWORD(lp));
            InvalidateRect(h, nullptr, FALSE);
            return 0;

          case WM_MOUSEMOVE:
            if(self->dragging)
            {
              self->set_gain_from((int)(short)LOWORD(lp));
              InvalidateRect(h, nullptr, FALSE);
            }
            return 0;

          case WM_LBUTTONUP:
            if(self->dragging)
            {
              self->dragging = false;
              ReleaseCapture();
              self->host.end_edit(self->host.ctx, gain_param);
              if(self->send_message)
                self->send_message(ui_to_processor{.value = self->gain()});
            }
            return 0;

          case WM_TIMER:
            InvalidateRect(h, nullptr, FALSE);
            return 0;

          case WM_PAINT: {
            PAINTSTRUCT ps;
            const HDC dc = BeginPaint(h, &ps);
            self->paint(dc);
            EndPaint(h, &ps);
            return 0;
          }

          default:
            return DefWindowProcW(h, msg, wp, lp);
        }
      }

      avnd::effect_container<CustomUiWindow>& fx;
      avnd::gui_host host{};
      HWND hwnd{};
      bool dragging{};
    };
  };
#elif defined(__linux__) && __has_include(<X11/Xlib.h>)
  struct ui
  {
    struct window
    {
      static constexpr int width = 360, height = 120;
      static constexpr int gain_param = 0; // flat index in parameter order

      explicit window(avnd::effect_container<CustomUiWindow>& fx)
          : fx{fx}
      {
      }

      ~window() { close(); }

      void open(avnd::gui_parent parent, avnd::gui_host h)
      {
        host = h;

        // The author's framework owns its display connection, like a
        // toolkit would; the host only hands us the parent window id.
        dpy = XOpenDisplay(nullptr);
        if(!dpy)
          return;
        const int screen = DefaultScreen(dpy);
        win = XCreateSimpleWindow(
            dpy, (::Window)(uintptr_t)parent.handle, 0, 0, width, height, 0,
            BlackPixel(dpy, screen), rgb(30, 30, 34));
        XSelectInput(
            dpy, win,
            ExposureMask | ButtonPressMask | ButtonReleaseMask
                | PointerMotionMask);
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

      // Host idle tick (effEditIdle, clap.timer-support, VST3 IRunLoop):
      // this is the author window's event loop *and* its repaint timer, so
      // host automation shows up without any callback plumbing.
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
                host.begin_edit(host.ctx, gain_param);
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
                host.end_edit(host.ctx, gain_param);
                if(send_message)
                  send_message(ui_to_processor{.value = gain()});
              }
              break;
            default:
              break;
          }
        }
        paint();
      }

      std::pair<int, int> size() const { return {width, height}; }

      // ---- Message bus endpoints (wired by the bindings) ----
      // UI thread -> processor; called on mouse release above.
      std::function<void(ui_to_processor)> send_message;
      // processor -> UI thread
      void process_message(processor_to_ui msg) { feedback = msg.applied; }
      float feedback{-1.f};

    private:
      static unsigned long rgb(int r, int g, int b)
      {
        // Direct pixel for the ubiquitous 24-bit TrueColor visual: an
        // example is allowed this shortcut, a real toolkit would XAllocColor.
        return ((unsigned long)r << 16) | ((unsigned long)g << 8)
               | (unsigned long)b;
      }

      float gain() const { return fx.effect.inputs.gain.value; }

      void set_gain_from(int x)
      {
        const float v = std::clamp(float(x) / float(width), 0.f, 1.f);
        // Write the model first, then tell the host: the bindings sample
        // the current value when broadcasting the automation point.
        fx.effect.inputs.gain.value = v;
        host.perform_edit(host.ctx, gain_param, v);
      }

      void paint()
      {
        XSetForeground(dpy, gc, rgb(30, 30, 34));
        XFillRectangle(dpy, win, gc, 0, 0, width, height);

        XSetForeground(dpy, gc, rgb(255, 176, 30));
        XFillRectangle(
            dpy, win, gc, 16, 32, (unsigned)((width - 32) * gain()),
            height - 64);

        char label[64];
        std::snprintf(label, sizeof(label), "gain: %.2f", gain());
        XSetForeground(dpy, gc, rgb(220, 220, 220));
        XDrawString(dpy, win, gc, 16, 20, label, (int)strlen(label));
        XFlush(dpy);
      }

      avnd::effect_container<CustomUiWindow>& fx;
      avnd::gui_host host{};
      Display* dpy{};
      ::Window win{};
      GC gc{};
      bool dragging{};
    };
  };
#endif
};
}
