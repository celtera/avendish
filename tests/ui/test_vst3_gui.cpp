/* SPDX-License-Identifier: GPL-3.0-or-later */

// End-to-end VST3 editor test: loads the .vst3 module, instantiates
// component + edit controller through the factory, opens the IPlugView in a
// real parent window, drives it with injected mouse messages, and verifies
// the IComponentHandler gesture flow and the controller parameter mirror.
// Runs on Windows (Win32 blit + SetTimer tick) and Linux (X11 blit + the
// host IRunLoop timer protocol).

#include <catch2/catch_test_macros.hpp>

#if (defined(_WIN32) || defined(__linux__)) && defined(AVND_TEST_VST3_PATH)

#include "gui_test_host.hpp"

#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/gui/iplugviewcontentscalesupport.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivsthostapplication.h>
#include <pluginterfaces/vst/ivstmessage.h>

#if defined(_WIN32)
#define AVND_TEST_DLOPEN(path) (void*)LoadLibraryA(path)
#define AVND_TEST_DLSYM(lib, name) (void*)GetProcAddress((HMODULE)lib, name)
#define AVND_TEST_DLCLOSE(lib) FreeLibrary((HMODULE)lib)
#else
#include <dlfcn.h>
#define AVND_TEST_DLOPEN(path) dlopen(path, RTLD_NOW | RTLD_LOCAL)
#define AVND_TEST_DLSYM(lib, name) dlsym(lib, name)
#define AVND_TEST_DLCLOSE(lib) dlclose(lib)
#endif

#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The plug-in module and this host each carry their own IID definitions.
namespace Steinberg
{
DEF_CLASS_IID(IPlugView)
DEF_CLASS_IID(IPlugFrame)
DEF_CLASS_IID(IPlugViewContentScaleSupport)
#if defined(__linux__)
namespace Linux
{
DEF_CLASS_IID(IEventHandler)
DEF_CLASS_IID(ITimerHandler)
DEF_CLASS_IID(IRunLoop)
}
#endif
namespace Vst
{
DEF_CLASS_IID(IComponent)
DEF_CLASS_IID(IAudioProcessor)
DEF_CLASS_IID(IEditController)
DEF_CLASS_IID(IComponentHandler)
DEF_CLASS_IID(IConnectionPoint)
DEF_CLASS_IID(IMessage)
DEF_CLASS_IID(IAttributeList)
DEF_CLASS_IID(IHostApplication)
}
}

namespace
{
using namespace Steinberg;

struct component_handler final : Vst::IComponentHandler
{
  int begins{}, performs{}, ends{}, restarts{};
  Vst::ParamID last_id{~0u};
  Vst::ParamValue last_value{-1.};

  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override
  {
    QUERY_INTERFACE(iid, obj, FUnknown::iid, Vst::IComponentHandler);
    QUERY_INTERFACE(iid, obj, Vst::IComponentHandler::iid, Vst::IComponentHandler);
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 2; }
  uint32 PLUGIN_API release() override { return 1; }

  tresult PLUGIN_API beginEdit(Vst::ParamID id) override
  {
    begins++;
    last_id = id;
    return kResultOk;
  }
  tresult PLUGIN_API performEdit(Vst::ParamID id, Vst::ParamValue v) override
  {
    performs++;
    last_id = id;
    last_value = v;
    return kResultOk;
  }
  tresult PLUGIN_API endEdit(Vst::ParamID id) override
  {
    ends++;
    return kResultOk;
  }
  tresult PLUGIN_API restartComponent(int32) override
  {
    restarts++;
    return kResultOk;
  }
};

// ---- Minimal host-side IMessage machinery for the connection points ----
struct test_attributes final : Vst::IAttributeList
{
  std::map<std::string, std::vector<char>> bins;

  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override
  {
    QUERY_INTERFACE(iid, obj, FUnknown::iid, Vst::IAttributeList);
    QUERY_INTERFACE(iid, obj, Vst::IAttributeList::iid, Vst::IAttributeList);
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 2; }
  uint32 PLUGIN_API release() override { return 1; }

  tresult PLUGIN_API setInt(AttrID, int64) override { return kResultFalse; }
  tresult PLUGIN_API getInt(AttrID, int64&) override { return kResultFalse; }
  tresult PLUGIN_API setFloat(AttrID, double) override { return kResultFalse; }
  tresult PLUGIN_API getFloat(AttrID, double&) override { return kResultFalse; }
  tresult PLUGIN_API setString(AttrID, const Vst::TChar*) override
  {
    return kResultFalse;
  }
  tresult PLUGIN_API getString(AttrID, Vst::TChar*, uint32) override
  {
    return kResultFalse;
  }
  tresult PLUGIN_API setBinary(AttrID id, const void* data, uint32 size) override
  {
    auto* bytes = (const char*)data;
    bins[id] = std::vector<char>(bytes, bytes + size);
    return kResultOk;
  }
  tresult PLUGIN_API getBinary(AttrID id, const void*& data, uint32& size) override
  {
    auto it = bins.find(id);
    if(it == bins.end())
      return kResultFalse;
    data = it->second.data();
    size = (uint32)it->second.size();
    return kResultOk;
  }
};

struct test_message final : Vst::IMessage
{
  std::string id;
  test_attributes attrs;
  int refs{1};

  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override
  {
    QUERY_INTERFACE(iid, obj, FUnknown::iid, Vst::IMessage);
    QUERY_INTERFACE(iid, obj, Vst::IMessage::iid, Vst::IMessage);
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return ++refs; }
  uint32 PLUGIN_API release() override
  {
    if(--refs == 0)
    {
      delete this;
      return 0;
    }
    return refs;
  }

  Steinberg::FIDString PLUGIN_API getMessageID() override { return id.c_str(); }
  void PLUGIN_API setMessageID(Steinberg::FIDString mid) override
  {
    id = mid ? mid : "";
  }
  Vst::IAttributeList* PLUGIN_API getAttributes() override { return &attrs; }
};

struct test_host_app final : Vst::IHostApplication
{
  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override
  {
    QUERY_INTERFACE(iid, obj, FUnknown::iid, Vst::IHostApplication);
    QUERY_INTERFACE(iid, obj, Vst::IHostApplication::iid, Vst::IHostApplication);
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 2; }
  uint32 PLUGIN_API release() override { return 1; }

  tresult PLUGIN_API getName(Vst::String128 name) override
  {
    static constexpr char16_t n[] = u"avnd-test-host";
    memcpy(name, n, sizeof(n));
    return kResultOk;
  }
  tresult PLUGIN_API createInstance(TUID cid, TUID iid, void** obj) override
  {
    if(memcmp(cid, Vst::IMessage::iid, sizeof(TUID)) == 0)
    {
      *obj = new test_message;
      return kResultOk;
    }
    *obj = nullptr;
    return kNoInterface;
  }
};

#if defined(__linux__)
// Host run loop per the VST3 Linux windowing contract: the view registers an
// ITimerHandler on attach and the host fires it from its event loop. Ticks
// are what drive editor->idle() (pugl event processing + rendering) and the
// controller-side bus pump.
struct test_run_loop final : Steinberg::Linux::IRunLoop
{
  std::vector<std::pair<Steinberg::Linux::ITimerHandler*, uint64>> timers;

  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override
  {
    QUERY_INTERFACE(iid, obj, FUnknown::iid, Steinberg::Linux::IRunLoop);
    QUERY_INTERFACE(
        iid, obj, Steinberg::Linux::IRunLoop::iid, Steinberg::Linux::IRunLoop);
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 2; }
  uint32 PLUGIN_API release() override { return 1; }

  tresult PLUGIN_API registerEventHandler(
      Steinberg::Linux::IEventHandler*, Steinberg::Linux::FileDescriptor) override
  {
    return kNotImplemented;
  }
  tresult PLUGIN_API
  unregisterEventHandler(Steinberg::Linux::IEventHandler*) override
  {
    return kNotImplemented;
  }
  tresult PLUGIN_API registerTimer(
      Steinberg::Linux::ITimerHandler* handler,
      Steinberg::Linux::TimerInterval ms) override
  {
    if(!handler)
      return kInvalidArgument;
    timers.emplace_back(handler, ms);
    return kResultOk;
  }
  tresult PLUGIN_API
  unregisterTimer(Steinberg::Linux::ITimerHandler* handler) override
  {
    std::erase_if(timers, [=](auto& t) { return t.first == handler; });
    return kResultOk;
  }

  void fire()
  {
    for(auto& [handler, interval] : timers)
      handler->onTimer();
  }
};

struct test_plug_frame final : IPlugFrame
{
  test_run_loop run_loop;

  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override
  {
    QUERY_INTERFACE(iid, obj, FUnknown::iid, IPlugFrame);
    QUERY_INTERFACE(iid, obj, IPlugFrame::iid, IPlugFrame);
    // The run loop lives on the frame object, as hosts commonly do it.
    if(Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Linux::IRunLoop::iid))
    {
      *obj = &run_loop;
      return kResultOk;
    }
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 2; }
  uint32 PLUGIN_API release() override { return 1; }

  tresult PLUGIN_API resizeView(IPlugView*, ViewRect*) override
  {
    return kResultOk;
  }
};
#endif

// Interposed connection point: forwards to the real peer while recording
// the message IDs that cross, so the test can assert on the wire protocol
// (ui->processor payloads, the UI-thread bus pump, processor->ui replies).
struct spy_connection final : Vst::IConnectionPoint
{
  Vst::IConnectionPoint* target{};
  std::vector<std::string> seen;

  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override
  {
    QUERY_INTERFACE(iid, obj, FUnknown::iid, Vst::IConnectionPoint);
    QUERY_INTERFACE(iid, obj, Vst::IConnectionPoint::iid, Vst::IConnectionPoint);
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 2; }
  uint32 PLUGIN_API release() override { return 1; }
  tresult PLUGIN_API connect(Vst::IConnectionPoint* o) override
  {
    return target->connect(o);
  }
  tresult PLUGIN_API disconnect(Vst::IConnectionPoint* o) override
  {
    return target->disconnect(o);
  }
  tresult PLUGIN_API notify(Vst::IMessage* m) override
  {
    if(m && m->getMessageID())
      seen.push_back(m->getMessageID());
    return target->notify(m);
  }
  bool saw(const char* id) const
  {
    for(auto& s : seen)
      if(s == id)
        return true;
    return false;
  }
};
}

TEST_CASE("vst3 editor: embed, interact, gestures", "[vst3_gui]")
{
  using namespace Steinberg;
  using namespace avnd_test_gui;

  void* lib = AVND_TEST_DLOPEN(AVND_TEST_VST3_PATH);
  REQUIRE(lib);
#if defined(_WIN32)
  if(auto init = (bool (*)())AVND_TEST_DLSYM(lib, "InitDll"))
    REQUIRE(init());
#else
  if(auto init = (bool (*)(void*))AVND_TEST_DLSYM(lib, "ModuleEntry"))
    REQUIRE(init(lib));
#endif

  const auto get_factory
      = (IPluginFactory * (*)()) AVND_TEST_DLSYM(lib, "GetPluginFactory");
  REQUIRE(get_factory);
  IPluginFactory* factory = get_factory();
  REQUIRE(factory);

  // Find the audio effect class + instantiate the component
  PClassInfo ci{};
  int32 fx_index = -1;
  for(int32 i = 0; i < factory->countClasses(); i++)
  {
    REQUIRE(factory->getClassInfo(i, &ci) == kResultOk);
    if(std::string_view{ci.category} == kVstAudioEffectClass)
    {
      fx_index = i;
      break;
    }
  }
  REQUIRE(fx_index >= 0);

  test_host_app host_app;

  Vst::IComponent* component{};
  REQUIRE(
      factory->createInstance(
          ci.cid, Vst::IComponent::iid, (void**)&component)
      == kResultOk);
  REQUIRE(component);
  REQUIRE(component->initialize(&host_app) == kResultOk);

  TUID controller_cid{};
  REQUIRE(component->getControllerClassId(controller_cid) == kResultOk);

  Vst::IEditController* controller{};
  REQUIRE(
      factory->createInstance(
          controller_cid, Vst::IEditController::iid, (void**)&controller)
      == kResultOk);
  REQUIRE(controller);
  REQUIRE(controller->initialize(&host_app) == kResultOk);

  // Connect the component and controller like a host does, so the message
  // bus can cross between them.
  Vst::IConnectionPoint* comp_cp{};
  Vst::IConnectionPoint* ctrl_cp{};
  REQUIRE(
      component->queryInterface(Vst::IConnectionPoint::iid, (void**)&comp_cp)
      == kResultOk);
  REQUIRE(
      controller->queryInterface(Vst::IConnectionPoint::iid, (void**)&ctrl_cp)
      == kResultOk);
  // Interpose spies so the test observes the wire protocol between the two.
  spy_connection to_controller; // component -> controller
  to_controller.target = ctrl_cp;
  spy_connection to_component; // controller -> component
  to_component.target = comp_cp;
  REQUIRE(comp_cp->connect(&to_controller) == kResultOk);
  REQUIRE(ctrl_cp->connect(&to_component) == kResultOk);

  component_handler handler;
  REQUIRE(controller->setComponentHandler(&handler) == kResultOk);

  // Initial state: Level (tag 0) is declared with init = 0.25
  CHECK(controller->getParamNormalized(0) == 0.25);

  // Create + attach the editor
  IPlugView* view = controller->createView(Vst::ViewType::kEditor);
  REQUIRE(view);
#if defined(_WIN32)
  REQUIRE(view->isPlatformTypeSupported(kPlatformTypeHWND) == kResultTrue);
#else
  REQUIRE(
      view->isPlatformTypeSupported(kPlatformTypeX11EmbedWindowID) == kResultTrue);
#endif

  ViewRect rect{};
  REQUIRE(view->getSize(&rect) == kResultTrue);
  CHECK(rect.getWidth() == 320);
  CHECK(rect.getHeight() == 220);

  const native_window parent
      = create_parent(rect.getWidth(), rect.getHeight(), L"avnd_vst3_gui_parent");
  REQUIRE(parent);

#if defined(_WIN32)
  // On Windows the view ticks itself with SetTimer through the message pump.
  const std::function<void()> tick{};
  REQUIRE(view->attached((void*)parent, kPlatformTypeHWND) == kResultOk);
#else
  // On Linux ticks come from the host run loop, queried from the IPlugFrame.
  test_plug_frame frame;
  REQUIRE(view->setFrame(&frame) == kResultTrue);
  const auto tick = [&] { frame.run_loop.fire(); };
  REQUIRE(
      view->attached((void*)(uintptr_t)parent, kPlatformTypeX11EmbedWindowID)
      == kResultOk);
  // The view must have registered its UI tick on the host run loop.
  REQUIRE(!frame.run_loop.timers.empty());
#endif

  pump_messages(300, tick);

  const native_window child = find_child(parent);
  REQUIRE(child);

  // Click + drag inside the custom widget
  mouse_move(child, 100, 50, false);
  mouse_press(child, 100, 50);
  pump_messages(200, tick);
  mouse_move(child, 150, 50, true);
  pump_messages(200, tick);
  mouse_release(child, 150, 50);
  pump_messages(200, tick);

  // Gestures reached the component handler with the normalized value,
  // and the controller mirror is current.
  CHECK(handler.begins == 1);
  CHECK(handler.ends == 1);
  CHECK(handler.performs >= 1);
  CHECK(handler.last_id == 0);
  CHECK(handler.last_value > 0.4);
  CHECK(controller->getParamNormalized(0) == handler.last_value);

  // Message bus round trip across the component/controller split: the
  // release queued a ui->processor message, sent over the connection as an
  // IMessage; the component drains it in process(), copies the value into
  // Gain, and answers with a feedback IMessage.
  Vst::IAudioProcessor* processor{};
  REQUIRE(
      component->queryInterface(Vst::IAudioProcessor::iid, (void**)&processor)
      == kResultOk);

  Vst::ProcessSetup setup{
      Vst::kRealtime, Vst::kSample32, 64, 48000.};
  REQUIRE(processor->setupProcessing(setup) == kResultOk);
  component->activateBus(Vst::kAudio, Vst::kInput, 0, true);
  component->activateBus(Vst::kAudio, Vst::kOutput, 0, true);
  REQUIRE(component->setActive(true) == kResultOk);

  float in_l[64], in_r[64], out_l[64]{}, out_r[64]{};
  for(int i = 0; i < 64; i++)
  {
    in_l[i] = 1.f;
    in_r[i] = 1.f;
  }
  float* ins[2] = {in_l, in_r};
  float* outs[2] = {out_l, out_r};
  Vst::AudioBusBuffers in_bus{};
  in_bus.numChannels = 2;
  in_bus.channelBuffers32 = ins;
  Vst::AudioBusBuffers out_bus{};
  out_bus.numChannels = 2;
  out_bus.channelBuffers32 = outs;

  Vst::ProcessData data{};
  data.processMode = Vst::kRealtime;
  data.symbolicSampleSize = Vst::kSample32;
  data.numSamples = 64;
  data.numInputs = 1;
  data.numOutputs = 1;
  data.inputs = &in_bus;
  data.outputs = &out_bus;
  REQUIRE(processor->process(data) == kResultOk);

  // The processor received the committed level over the bus and used it
  // this very block: gain was set to the committed value by
  // process_message. The Level *parameter* on the component still holds its
  // 0.25 init — this minimal host does not route performEdit back as
  // parameter changes the way a real host would.
  CHECK(out_l[0] == 1.f * (float)handler.last_value * 0.25f);

  // Assert on the observed wire protocol:
  // - the committed widget value crossed as a ui->processor IMessage
  //   (its payload has std::string + std::vector members — serialized);
  // - the controller pumps the component from the view timer (the
  //   processor never sends from the audio thread);
  // - the processor's reply crossed back after a pump.
  pump_messages(100, tick);
  CHECK(to_component.saw("avnd_ui_to_processor"));
  CHECK(to_component.saw("avnd_bus_pump"));
  CHECK(to_controller.saw("avnd_processor_to_ui"));

  component->setActive(false);
  processor->release();

  // Host-driven automation flows back into the editor without incident.
  // The mirror stores float, so compare with a float-precision tolerance.
  REQUIRE(controller->setParamNormalized(0, 0.1) == kResultTrue);
  CHECK(std::abs(controller->getParamNormalized(0) - 0.1) < 1e-6);
  pump_messages(100, tick);

  // HiDPI: the host announces a fractional content scale while the editor
  // is open. The view reports the scaled physical size and the embedded
  // window follows it (the render covers the whole new surface — a stale
  // edge sliver here is the classic partial-clear bug).
  IPlugViewContentScaleSupport* scale_support{};
  REQUIRE(
      view->queryInterface(
          IPlugViewContentScaleSupport::iid, (void**)&scale_support)
      == kResultOk);
  REQUIRE(scale_support->setContentScaleFactor(1.25f) == kResultTrue);
  pump_messages(200, tick);

  ViewRect scaled{};
  REQUIRE(view->getSize(&scaled) == kResultTrue);
  CHECK(scaled.getWidth() == 400);  // 320 * 1.25
  CHECK(scaled.getHeight() == 275); // 220 * 1.25
  {
    const auto [cw_px, ch_px] = window_size(child);
    CHECK(cw_px == 400);
    CHECK(ch_px == 275);
  }
  scale_support->release();

  REQUIRE(view->removed() == kResultOk);
#if !defined(_WIN32)
  // The IRunLoop timer must be unregistered when the view is removed.
  CHECK(frame.run_loop.timers.empty());
#endif
  view->release();
  destroy_parent(parent);

  comp_cp->disconnect(&to_controller);
  ctrl_cp->disconnect(&to_component);
  comp_cp->release();
  ctrl_cp->release();

  controller->terminate();
  controller->release();
  component->terminate();
  component->release();
  factory->release();
#if defined(_WIN32)
  if(auto exit_dll = (bool (*)())AVND_TEST_DLSYM(lib, "ExitDll"))
    exit_dll();
#else
  if(auto exit_mod = (bool (*)())AVND_TEST_DLSYM(lib, "ModuleExit"))
    exit_mod();
#endif
  AVND_TEST_DLCLOSE(lib);
}

#else

TEST_CASE("vst3 editor: embed, interact, gestures", "[vst3_gui]")
{
  SKIP("vst3 gui test needs Win32 or X11");
}

#endif
