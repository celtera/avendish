#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// A self-contained VST3 editor built on VSTGUI. Enabled when the addon is
// compiled with AVND_VST3_VSTGUI (see avendish.vst3.cmake). It hosts a VSTGUI
// CFrame inside a host-provided window and auto-lays-out one self-drawing
// widget per parameter (knob for continuous, checkbox for toggles), bound to
// the controller through the standard IEditController + IComponentHandler API.
// No Qt, no bitmaps, no uidesc: it works for any avnd plugin out of the box.

#if defined(AVND_VST3_VSTGUI)

#include <avnd/binding/vst3/controller_base.hpp>

#include <base/source/fstring.h>
#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>

#include <vstgui/vstgui.h>
#include <vstgui/lib/controls/cknob.h>
#include <vstgui/lib/controls/cbuttons.h>
#include <vstgui/lib/controls/ctextlabel.h>

#if defined(__linux__)
#include <vstgui/lib/platform/linux/x11frame.h>
#include <vstgui/lib/platform/iplatformframe.h>
#include <base/source/fobject.h>
#include <pluginterfaces/base/smartpointer.h>
#include <vector>
#endif

#include <atomic>
#include <cstring>

namespace stv3
{

#if defined(__linux__)
// Bridges the host's Steinberg::Linux::IRunLoop (from IPlugFrame) to VSTGUI's
// X11 run loop, required for VSTGUI to handle events inside a VST3 host on
// Linux. Adapted from VSTGUI's own vst3editor.cpp.
class Vst3X11RunLoop final
    : public VSTGUI::X11::IRunLoop
    , public VSTGUI::AtomicReferenceCounted
{
public:
  struct EventHandler : Steinberg::Linux::IEventHandler, public Steinberg::FObject
  {
    VSTGUI::X11::IEventHandler* handler{nullptr};
    void PLUGIN_API onFDIsSet(Steinberg::Linux::FileDescriptor) override
    { if(handler) handler->onEvent(); }
    DELEGATE_REFCOUNT(Steinberg::FObject)
    DEFINE_INTERFACES
      DEF_INTERFACE(Steinberg::Linux::IEventHandler)
    END_DEFINE_INTERFACES(Steinberg::FObject)
  };
  struct TimerHandler : Steinberg::Linux::ITimerHandler, public Steinberg::FObject
  {
    VSTGUI::X11::ITimerHandler* handler{nullptr};
    void PLUGIN_API onTimer() final { if(handler) handler->onTimer(); }
    DELEGATE_REFCOUNT(Steinberg::FObject)
    DEFINE_INTERFACES
      DEF_INTERFACE(Steinberg::Linux::ITimerHandler)
    END_DEFINE_INTERFACES(Steinberg::FObject)
  };

  bool registerEventHandler(int fd, VSTGUI::X11::IEventHandler* handler) final
  {
    if(!runLoop) return false;
    auto h = Steinberg::owned(new EventHandler());
    h->handler = handler;
    if(runLoop->registerEventHandler(h, fd) == Steinberg::kResultTrue)
    { eventHandlers.push_back(h); return true; }
    return false;
  }
  bool unregisterEventHandler(VSTGUI::X11::IEventHandler* handler) final
  {
    if(!runLoop) return false;
    for(auto it = eventHandlers.begin(); it != eventHandlers.end(); ++it)
      if((*it)->handler == handler)
      { runLoop->unregisterEventHandler(*it); eventHandlers.erase(it); return true; }
    return false;
  }
  bool registerTimer(uint64_t interval, VSTGUI::X11::ITimerHandler* handler) final
  {
    if(!runLoop) return false;
    auto h = Steinberg::owned(new TimerHandler());
    h->handler = handler;
    if(runLoop->registerTimer(h, interval) == Steinberg::kResultTrue)
    { timerHandlers.push_back(h); return true; }
    return false;
  }
  bool unregisterTimer(VSTGUI::X11::ITimerHandler* handler) final
  {
    if(!runLoop) return false;
    for(auto it = timerHandlers.begin(); it != timerHandlers.end(); ++it)
      if((*it)->handler == handler)
      { runLoop->unregisterTimer(*it); timerHandlers.erase(it); return true; }
    return false;
  }

  explicit Vst3X11RunLoop(Steinberg::FUnknown* rl) : runLoop(rl) {}

private:
  std::vector<Steinberg::IPtr<EventHandler>> eventHandlers;
  std::vector<Steinberg::IPtr<TimerHandler>> timerHandlers;
  Steinberg::FUnknownPtr<Steinberg::Linux::IRunLoop> runLoop;
};
#endif

class VstGuiEditor final
    : public Steinberg::IPlugView
    , public VSTGUI::IControlListener
{
public:
  explicit VstGuiEditor(ControllerCommon* c)
      : controller{c}
  {
    // Hosts call getSize() before attached() to size the window: compute now.
    computeSize();
  }

  // ---------------- IUnknown ----------------
  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override
  {
    using namespace Steinberg;
    if(FUnknownPrivate::iidEqual(iid, FUnknown::iid)
       || FUnknownPrivate::iidEqual(iid, IPlugView::iid))
    {
      *obj = static_cast<IPlugView*>(this);
      addRef();
      return kResultTrue;
    }
    *obj = nullptr;
    return kNoInterface;
  }
  Steinberg::uint32 PLUGIN_API addRef() override { return ++m_ref; }
  Steinberg::uint32 PLUGIN_API release() override
  {
    if(--m_ref == 0) { delete this; return 0; }
    return m_ref;
  }

  // ---------------- IPlugView ----------------
  Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override
  {
    return std::strcmp(type, currentPlatformType()) == 0 ? Steinberg::kResultTrue
                                                         : Steinberg::kResultFalse;
  }

  Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString /*type*/) override
  {
    computeSize();
    frame = new VSTGUI::CFrame(VSTGUI::CRect(0, 0, m_width, m_height), nullptr);
    frame->setBackgroundColor(VSTGUI::CColor{32, 32, 34, 255});

#if defined(_WIN32)
    frame->open(parent, VSTGUI::PlatformType::kHWND);
#elif defined(__APPLE__)
    frame->open(parent, VSTGUI::PlatformType::kNSView);
#else
    // X11 needs the host run loop wired into VSTGUI, or event handling crashes.
    VSTGUI::X11::FrameConfig x11config;
    x11config.runLoop = VSTGUI::owned(new Vst3X11RunLoop(plugFrame));
    frame->open(parent, VSTGUI::PlatformType::kX11EmbedWindowID, &x11config);
#endif
    buildControls();
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult PLUGIN_API removed() override
  {
    if(frame)
    {
      frame->close(); // forgets itself
      frame = nullptr;
    }
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult PLUGIN_API onWheel(float) override { return Steinberg::kResultFalse; }
  Steinberg::tresult PLUGIN_API onKeyDown(Steinberg::char16, Steinberg::int16, Steinberg::int16) override
  { return Steinberg::kResultFalse; }
  Steinberg::tresult PLUGIN_API onKeyUp(Steinberg::char16, Steinberg::int16, Steinberg::int16) override
  { return Steinberg::kResultFalse; }

  Steinberg::tresult PLUGIN_API getSize(Steinberg::ViewRect* size) override
  {
    size->left = 0; size->top = 0; size->right = m_width; size->bottom = m_height;
    return Steinberg::kResultTrue;
  }
  Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect*) override { return Steinberg::kResultTrue; }
  Steinberg::tresult PLUGIN_API onFocus(Steinberg::TBool) override { return Steinberg::kResultTrue; }
  Steinberg::tresult PLUGIN_API setFrame(Steinberg::IPlugFrame* f) override
  {
    plugFrame = f;
    return Steinberg::kResultTrue;
  }
  Steinberg::tresult PLUGIN_API canResize() override { return Steinberg::kResultFalse; }
  Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect*) override
  { return Steinberg::kResultTrue; }

  // ---------------- IControlListener ----------------
  void valueChanged(VSTGUI::CControl* pControl) override
  {
    auto tag = static_cast<Steinberg::Vst::ParamID>(pControl->getTag());
    auto v = static_cast<Steinberg::Vst::ParamValue>(pControl->getValueNormalized());
    if(controller->componentHandler)
    {
      controller->componentHandler->beginEdit(tag);
      controller->componentHandler->performEdit(tag, v);
      controller->componentHandler->endEdit(tag);
    }
    edit_controller()->setParamNormalized(tag, v);
  }

private:
  static Steinberg::FIDString currentPlatformType()
  {
#if defined(_WIN32)
    return Steinberg::kPlatformTypeHWND;
#elif defined(__APPLE__)
    return Steinberg::kPlatformTypeNSView;
#else
    return Steinberg::kPlatformTypeX11EmbedWindowID;
#endif
  }

  Steinberg::Vst::IEditController* edit_controller()
  {
    return static_cast<Steinberg::Vst::IEditController*>(controller);
  }

  void computeSize()
  {
    const int n = edit_controller()->getParameterCount();
    const int rows = (n + cols - 1) / cols;
    m_width = cols * cell_w + margin * 2;
    m_height = (rows > 0 ? rows : 1) * cell_h + margin * 2;
    if(m_width < 240) m_width = 240;
    if(m_height < 140) m_height = 140;
  }

  void buildControls()
  {
    auto* ec = edit_controller();
    const int n = ec->getParameterCount();
    int col = 0, row = 0;
    for(int i = 0; i < n; ++i)
    {
      Steinberg::Vst::ParameterInfo info{};
      if(ec->getParameterInfo(i, info) != Steinberg::kResultTrue)
        continue;

      const VSTGUI::CCoord x = margin + col * cell_w;
      const VSTGUI::CCoord y = margin + row * cell_h;

      VSTGUI::CControl* ctl = nullptr;
      if(info.stepCount == 1)
      {
        // Toggle -> self-drawing checkbox.
        VSTGUI::CRect rc(x + 8, y + 24, x + cell_w - 8, y + 48);
        ctl = new VSTGUI::CCheckBox(rc, this, info.id, title_of(info).data());
      }
      else
      {
        // Continuous -> self-drawing knob.
        VSTGUI::CRect rc(x + (cell_w - 56) / 2, y + 20, x + (cell_w - 56) / 2 + 56, y + 76);
        auto* knob = new VSTGUI::CKnob(
            rc, this, info.id, nullptr, nullptr, VSTGUI::CPoint(0, 0),
            VSTGUI::CKnob::kCoronaDrawing | VSTGUI::CKnob::kHandleCircleDrawing);
        ctl = knob;

        // Name label above the knob.
        VSTGUI::CRect lr(x, y + 2, x + cell_w, y + 18);
        const std::string lbl_txt = title_of(info);
        auto* lbl = new VSTGUI::CTextLabel(lr, lbl_txt.c_str());
        lbl->setFontColor(VSTGUI::kWhiteCColor);
        lbl->setBackColor(VSTGUI::kTransparentCColor);
        lbl->setFrameColor(VSTGUI::kTransparentCColor);
        frame->addView(lbl);
      }

      ctl->setValueNormalized(static_cast<float>(ec->getParamNormalized(info.id)));
      frame->addView(ctl);

      if(++col >= cols) { col = 0; ++row; }
    }
  }

  static std::string title_of(const Steinberg::Vst::ParameterInfo& info)
  {
    Steinberg::String s(info.title);
    s.toMultiByte(Steinberg::kCP_Utf8);
    return std::string(s.text8() ? s.text8() : "");
  }

  ControllerCommon* controller{};
  VSTGUI::CFrame* frame{};
  Steinberg::IPlugFrame* plugFrame{};
  std::atomic<Steinberg::uint32> m_ref{1};

  static constexpr int cols = 4;
  static constexpr int cell_w = 90;
  static constexpr int cell_h = 96;
  static constexpr int margin = 12;
  int m_width = 240;
  int m_height = 140;
};

}

#endif
