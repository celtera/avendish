#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * Shared platform layer for the end-to-end editor tests: a real parent
 * window, a message/event pump with a per-tick host callback, child-window
 * discovery, synthetic mouse input aimed at the embedded editor, and a
 * best-effort PPM capture of what the editor blitted.
 *
 * Win32: the pump dispatches the thread queue (pugl's SetTimer path); input
 * is posted as WM_* messages to the child HWND.
 * X11: the test host owns its own Display connection; the plug-in's editor
 * runs on pugl's separate connection, driven by the per-tick callback (the
 * host timer the bindings register: clap.timer-support / effEditIdle /
 * VST3 IRunLoop). Input is delivered with XSendEvent to the child window —
 * pugl translates synthetic core events like real ones.
 */

#include <functional>

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

#include <cstdio>
#include <cstring>

namespace avnd_test_gui
{
using native_window = HWND;

inline void pump_messages(int ms, const std::function<void()>& tick = {})
{
  const DWORD end = GetTickCount() + ms;
  for(;;)
  {
    MSG msg;
    while(PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    if(tick)
      tick();
    if(GetTickCount() >= end)
      break;
    Sleep(5);
  }
}

inline native_window create_parent(int w, int h, const wchar_t* class_name)
{
  WNDCLASSW wc{};
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = class_name;
  RegisterClassW(&wc);
  RECT r{0, 0, w, h};
  AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  return CreateWindowExW(
      0, wc.lpszClassName, L"avnd gui test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr,
      nullptr, wc.hInstance, nullptr);
}

inline void destroy_parent(native_window w)
{
  DestroyWindow(w);
}

namespace detail
{
struct child_finder
{
  HWND child{};
  static BOOL CALLBACK cb(HWND hwnd, LPARAM p)
  {
    ((child_finder*)p)->child = hwnd;
    return FALSE;
  }
};
}

inline native_window find_child(native_window parent)
{
  detail::child_finder finder;
  EnumChildWindows(parent, &detail::child_finder::cb, (LPARAM)&finder);
  return finder.child;
}

inline std::pair<int, int> window_size(native_window w)
{
  RECT rc{};
  GetClientRect(w, &rc);
  return {(int)rc.right, (int)rc.bottom};
}

inline void mouse_move(native_window w, int x, int y, bool button_held)
{
  PostMessageW(w, WM_MOUSEMOVE, button_held ? MK_LBUTTON : 0, MAKELPARAM(x, y));
}

inline void mouse_press(native_window w, int x, int y)
{
  PostMessageW(w, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
}

inline void mouse_release(native_window w, int x, int y)
{
  PostMessageW(w, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
}

// Best-effort visual artifact: capture the editor window into a PPM.
inline void capture_window(native_window hwnd, const char* path)
{
  RECT rc{};
  GetClientRect(hwnd, &rc);
  const int w = rc.right, h = rc.bottom;
  if(w <= 0 || h <= 0)
    return;

  const HDC win_dc = GetDC(hwnd);
  const HDC mem_dc = CreateCompatibleDC(win_dc);
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits{};
  if(const HBITMAP bmp
     = CreateDIBSection(win_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0))
  {
    const auto old = SelectObject(mem_dc, bmp);
    // PW_RENDERFULLCONTENT (2): ask DWM for the actual content
    if(!PrintWindow(hwnd, mem_dc, 2))
      BitBlt(mem_dc, 0, 0, w, h, win_dc, 0, 0, SRCCOPY);
    if(std::FILE* f = std::fopen(path, "wb"))
    {
      std::fprintf(f, "P6\n%d %d\n255\n", w, h);
      auto* px = (const unsigned char*)bits;
      for(int i = 0; i < w * h; i++)
      {
        const unsigned char rgb[3] = {px[i * 4 + 2], px[i * 4 + 1], px[i * 4 + 0]};
        std::fwrite(rgb, 1, 3, f);
      }
      std::fclose(f);
    }
    SelectObject(mem_dc, old);
    DeleteObject(bmp);
  }
  DeleteDC(mem_dc);
  ReleaseDC(hwnd, win_dc);
}
}

#elif defined(__linux__) || defined(__FreeBSD__)

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <chrono>
#include <cstdio>
#include <thread>

namespace avnd_test_gui
{
using native_window = ::Window;

namespace detail
{
// One connection for the whole test binary; the plug-in editor talks to the
// server through pugl's own connection.
inline Display* dpy()
{
  static Display* d = XOpenDisplay(nullptr);
  return d;
}
}

// The editor makes progress through the per-tick host callback (clap timer,
// effEditIdle, IRunLoop timer, ...): pugl only processes its connection
// inside editor->idle(). The test host itself has nothing to dispatch, but
// drain our connection anyway so the server never blocks on us.
inline void pump_messages(int ms, const std::function<void()>& tick = {})
{
  auto* d = detail::dpy();
  const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  for(;;)
  {
    if(d)
    {
      while(XPending(d))
      {
        XEvent ev;
        XNextEvent(d, &ev);
      }
    }
    if(tick)
      tick();
    if(std::chrono::steady_clock::now() >= end)
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

inline native_window create_parent(int w, int h, const wchar_t* /* class_name */)
{
  auto* d = detail::dpy();
  if(!d)
    return 0;
  const int screen = DefaultScreen(d);
  const ::Window win = XCreateSimpleWindow(
      d, RootWindow(d, screen), 0, 0, (unsigned)w, (unsigned)h, 0,
      BlackPixel(d, screen), BlackPixel(d, screen));
  XStoreName(d, win, "avnd gui test");
  XSelectInput(d, win, StructureNotifyMask);
  XMapWindow(d, win);
  // Wait for the map so the plug-in can parent into a viewable window.
  for(int i = 0; i < 200; i++)
  {
    XWindowAttributes attrs{};
    XGetWindowAttributes(d, win, &attrs);
    if(attrs.map_state == IsViewable)
      break;
    pump_messages(5);
  }
  XSync(d, False);
  return win;
}

inline void destroy_parent(native_window w)
{
  if(auto* d = detail::dpy(); d && w)
  {
    XDestroyWindow(d, w);
    XSync(d, False);
  }
}

inline native_window find_child(native_window parent)
{
  auto* d = detail::dpy();
  if(!d || !parent)
    return 0;
  XSync(d, False);
  ::Window root{}, par{};
  ::Window* children{};
  unsigned int n{};
  ::Window found{};
  if(XQueryTree(d, parent, &root, &par, &children, &n) && n > 0)
    found = children[0];
  if(children)
    XFree(children);
  return found;
}

inline std::pair<int, int> window_size(native_window w)
{
  auto* d = detail::dpy();
  if(!d || !w)
    return {0, 0};
  XWindowAttributes attrs{};
  XGetWindowAttributes(d, w, &attrs);
  return {attrs.width, attrs.height};
}

namespace detail
{
inline XEvent
make_pointer_event(native_window w, int type, int x, int y, bool button_held)
{
  auto* d = dpy();
  XEvent ev{};
  ev.xany.display = d;
  ev.xany.window = w;
  if(type == MotionNotify)
  {
    ev.type = MotionNotify;
    ev.xmotion.window = w;
    ev.xmotion.root = DefaultRootWindow(d);
    ev.xmotion.x = x;
    ev.xmotion.y = y;
    ev.xmotion.state = button_held ? Button1Mask : 0;
    ev.xmotion.time = CurrentTime;
    ev.xmotion.same_screen = True;
  }
  else
  {
    ev.type = type;
    ev.xbutton.window = w;
    ev.xbutton.root = DefaultRootWindow(d);
    ev.xbutton.x = x;
    ev.xbutton.y = y;
    ev.xbutton.button = Button1;
    ev.xbutton.state = type == ButtonRelease ? Button1Mask : 0;
    ev.xbutton.time = CurrentTime;
    ev.xbutton.same_screen = True;
  }
  return ev;
}

inline void send_event(native_window w, XEvent ev, long mask)
{
  auto* d = dpy();
  XSendEvent(d, w, True, mask, &ev);
  XFlush(d);
}
}

inline void mouse_move(native_window w, int x, int y, bool button_held)
{
  detail::send_event(
      w, detail::make_pointer_event(w, MotionNotify, x, y, button_held),
      PointerMotionMask);
}

inline void mouse_press(native_window w, int x, int y)
{
  detail::send_event(
      w, detail::make_pointer_event(w, ButtonPress, x, y, false), ButtonPressMask);
}

inline void mouse_release(native_window w, int x, int y)
{
  detail::send_event(
      w, detail::make_pointer_event(w, ButtonRelease, x, y, true),
      ButtonReleaseMask);
}

// Best-effort visual artifact: capture the editor window into a PPM.
inline void capture_window(native_window w, const char* path)
{
  auto* d = detail::dpy();
  if(!d || !w)
    return;
  XSync(d, False);
  XWindowAttributes attrs{};
  XGetWindowAttributes(d, w, &attrs);
  if(attrs.width <= 0 || attrs.height <= 0 || attrs.map_state != IsViewable)
    return;
  XImage* img
      = XGetImage(d, w, 0, 0, attrs.width, attrs.height, AllPlanes, ZPixmap);
  if(!img)
    return;
  if(std::FILE* f = std::fopen(path, "wb"))
  {
    std::fprintf(f, "P6\n%d %d\n255\n", attrs.width, attrs.height);
    for(int y = 0; y < attrs.height; y++)
    {
      for(int x = 0; x < attrs.width; x++)
      {
        const unsigned long px = XGetPixel(img, x, y);
        const unsigned char rgb[3]
            = {(unsigned char)((px & img->red_mask) >> 16),
               (unsigned char)((px & img->green_mask) >> 8),
               (unsigned char)(px & img->blue_mask)};
        std::fwrite(rgb, 1, 3, f);
      }
    }
    std::fclose(f);
  }
  XDestroyImage(img);
}
}

#endif
