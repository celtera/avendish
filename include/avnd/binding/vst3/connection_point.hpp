#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/vst3/metadata.hpp>
#include <base/source/fstring.h>
#include <pluginterfaces/vst/ivstmessage.h>
#include <pluginterfaces/vst/ivsthostapplication.h>
#include <cstdio>
namespace stv3
{

class ConnectionPoint : public Steinberg::Vst::IConnectionPoint
{
public:
  Steinberg::IPtr<IConnectionPoint> peerConnection{};

  Steinberg::tresult receiveText(const char* text)
  {
    return Steinberg::kResultOk;
  }

  Steinberg::tresult connect(IConnectionPoint* other) final override
  {
    if (!other)
      return Steinberg::kInvalidArgument;

    if (peerConnection)
      return Steinberg::kResultFalse;

    peerConnection = other;
    return Steinberg::kResultOk;
  }

  Steinberg::tresult disconnect(IConnectionPoint* other) final override
  {
    if (peerConnection && other == peerConnection)
    {
      peerConnection = nullptr;
      return Steinberg::kResultOk;
    }
    return Steinberg::kResultFalse;
  }

  Steinberg::tresult notify(Steinberg::Vst::IMessage* message) final override
  {
    if (!message)
      return Steinberg::kInvalidArgument;

    return Steinberg::kResultFalse;
  }
};

}
