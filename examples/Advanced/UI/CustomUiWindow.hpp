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
 * The window is Windows-only for brevity; on other platforms the plug-in
 * simply has no editor (the concept is not satisfied, bindings skip it).
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
#endif
};
}
