/* SPDX-License-Identifier: GPL-3.0-or-later */

// Tier C editor -- Windows implementation. A raw Win32 child window painted
// with GDI, living behind the pimpl forward-declared in CustomUiWindow.hpp.
// See that header for the contract; the Linux sibling is CustomUiWindow.x11.cpp.

#include "CustomUiWindow.hpp"

#if defined(_WIN32)

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN 1
#endif
#if !defined(NOMINMAX)
#define NOMINMAX 1
#endif
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace examples
{
// The whole framework side of the window: state, painting and the wndproc.
// Reads the gain straight off the effect model and drives the host's
// automation-gesture hooks -- exactly as a real toolkit-based editor would.
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

    WNDCLASSW wc{};
    wc.lpfnWndProc = &impl::wndproc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"avnd_custom_ui_window_example";
    RegisterClassW(&wc); // idempotent: re-registration just fails

    hwnd = CreateWindowExW(
        0, wc.lpszClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, window::width,
        window::height, (HWND)parent.handle, nullptr, wc.hInstance, this);

    // Repaint tick, dispatched by the host's message pump: host automation
    // moves the bar without any callback plumbing.
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

  // Host idle tick (effEditIdle, clap.timer-support): nothing to do, WM_TIMER
  // already drives us through the message pump.
  void idle() { }

private:
  float gain() const { return fx.effect.inputs.gain.value; }

  void set_gain_from(int x)
  {
    const float v = std::clamp(float(x) / float(window::width), 0.f, 1.f);
    // Write the model first, then tell the host: the bindings sample the
    // current value when broadcasting the automation point.
    fx.effect.inputs.gain.value = v;
    host.perform_edit(host.ctx, window::gain_param, v);
  }

  void paint(HDC dc)
  {
    RECT rc{0, 0, window::width, window::height};
    const HBRUSH bg = CreateSolidBrush(RGB(30, 30, 34));
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    RECT bar{
        16, 32, 16 + (int)((window::width - 32) * gain()), window::height - 32};
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
    auto* self = (impl*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if(!self)
      return DefWindowProcW(h, msg, wp, lp);

    switch(msg)
    {
      case WM_LBUTTONDOWN:
        SetCapture(h);
        self->dragging = true;
        self->host.begin_edit(self->host.ctx, window::gain_param);
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
          self->host.end_edit(self->host.ctx, window::gain_param);
          if(self->owner.send_message)
            self->owner.send_message(ui_to_processor{.value = self->gain()});
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

  window& owner;
  avnd::effect_container<CustomUiWindow>& fx;
  avnd::gui_host host{};
  HWND hwnd{};
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
