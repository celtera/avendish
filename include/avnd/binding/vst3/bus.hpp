#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

/**
 * UI <-> processor message bus over VST3 IMessage (CUSTOM_UI_PLAN.md §8):
 * unlike CLAP/VST2 the editor lives with the edit controller, a different
 * object from the processing component (possibly out-of-process), so bus
 * payloads cross through the host's connection points.
 *
 * v1 carries trivially-copyable messages as a binary attribute; non-trivial
 * payloads (std::string / std::vector members...) need a serializer and are
 * not routed yet -- such plug-ins keep the view-local behaviour.
 */

#include <avnd/binding/ui/message_transport.hpp>

#include <pluginterfaces/vst/ivsthostapplication.h>
#include <pluginterfaces/vst/ivstmessage.h>

#include <type_traits>

namespace stv3
{
static constexpr const char* bus_ui_to_processor_id = "avnd_ui_to_processor";
static constexpr const char* bus_processor_to_ui_id = "avnd_processor_to_ui";
static constexpr const char* bus_payload_key = "avnd_payload";

namespace detail
{
template <typename Msg>
static constexpr bool busable = !std::is_void_v<Msg> && [] {
  if constexpr(!std::is_void_v<Msg>)
    return std::is_trivially_copyable_v<Msg>;
  else
    return false;
}();
}

template <typename T>
static constexpr bool bus_to_processor_enabled
    = avnd::ui_sends_to_processor<T>
      && detail::busable<avnd::any_ui_to_proc_msg_t<T>>;

template <typename T>
static constexpr bool bus_to_ui_enabled
    = avnd::processor_sends_to_ui<T>
      && detail::busable<avnd::any_proc_to_ui_msg_t<T>>;

// Send a message through a connection point. Allocates the IMessage from
// the host application found in `context`.
template <typename Msg>
inline bool send_bus_message(
    Steinberg::FUnknown* context, Steinberg::Vst::IConnectionPoint* peer,
    const char* message_id, const Msg& msg)
{
  using namespace Steinberg;
  using namespace Steinberg::Vst;
  if(!context || !peer)
    return false;

  IHostApplication* app{};
  if(context->queryInterface(IHostApplication::iid, (void**)&app) != kResultOk
     || !app)
    return false;

  TUID iid;
  memcpy(iid, IMessage::iid, sizeof(TUID));
  IMessage* m{};
  const bool ok
      = app->createInstance(iid, iid, (void**)&m) == kResultOk && m != nullptr;
  if(ok)
  {
    m->setMessageID(message_id);
    if(auto* attrs = m->getAttributes())
      attrs->setBinary(bus_payload_key, &msg, sizeof(Msg));
    peer->notify(m);
    m->release();
  }
  app->release();
  return ok;
}

// Extract a message of the given type; returns false when the IMessage is
// not ours or the payload does not match.
template <typename Msg>
inline bool read_bus_message(
    Steinberg::Vst::IMessage& m, const char* message_id, Msg& out)
{
  using namespace Steinberg;
  if(!m.getMessageID() || strcmp(m.getMessageID(), message_id) != 0)
    return false;
  auto* attrs = m.getAttributes();
  if(!attrs)
    return false;
  const void* data{};
  Steinberg::uint32 size{};
  if(attrs->getBinary(bus_payload_key, data, size) != kResultOk || !data
     || size != sizeof(Msg))
    return false;
  memcpy(&out, data, sizeof(Msg));
  return true;
}
}
