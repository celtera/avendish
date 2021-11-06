#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <vst3/metadata.hpp>
#include <vst3/connection_point.hpp>
#include <vst3/helpers.hpp>

#include <base/source/fstreamer.h>
#include <base/source/fstring.h>
#include <pluginterfaces/base/fplatform.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstevents.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>

#include <cassert>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <array>

namespace stv3
{
// Taken from VST3 SDK
inline void** getChannelBuffersPointer(
    const Steinberg::Vst::ProcessSetup& processSetup,
    const Steinberg::Vst::AudioBusBuffers& bufs)
{
  using namespace Steinberg::Vst;
  return (processSetup.symbolicSampleSize == kSample32)
      ? (void**)bufs.channelBuffers32
      : (void**)bufs.channelBuffers64;
}

inline uint32 getSampleFramesSizeInBytes (const Steinberg::Vst::ProcessSetup& processSetup, int32 numSamples)
{
  using namespace Steinberg::Vst;
  return numSamples * (processSetup.symbolicSampleSize == kSample32
                       ? sizeof(Sample32)
                       : sizeof(Sample64));
}

struct AudioBusInfo
{
  Steinberg::Vst::MediaType mediaType;
  Steinberg::Vst::BusDirection direction;
  int32 channelCount;
  Steinberg::Vst::String128 name;
  Steinberg::Vst::BusType busType;
  uint32 flags;

  Steinberg::Vst::SpeakerArrangement arrangement{};
};

struct EventBusInfo
{
  Steinberg::Vst::MediaType mediaType;
  Steinberg::Vst::BusDirection direction;
  int32 channelCount;
  Steinberg::Vst::String128 name;
  Steinberg::Vst::BusType busType;
  uint32 flags;
};

class ComponentCommon
    : public Steinberg::Vst::IComponent
    , public Steinberg::IDependent
    , public stv3::ConnectionPoint
{
public:
  Steinberg::IPtr<FUnknown> hostContext{};
  void update(FUnknown* /*changedUnknown*/, int32 /*message*/) final override
  {
  }

  tresult initialize(FUnknown* context) final override
  {
    using namespace Steinberg;
    {
      // check if already initialized
      if (hostContext)
        return kResultFalse;

      hostContext = context;
    }

    return kResultOk;
  }

  tresult terminate() final override
  {
    using namespace Steinberg;
    // release host interfaces
    hostContext = nullptr;

    // in case host did not disconnect us,
    // release peer now
    if (peerConnection)
    {
      peerConnection->disconnect(this);
      peerConnection = nullptr;
    }

    return kResultOk;
  }
  Steinberg::tresult setIoMode(Steinberg::Vst::IoMode mode) final override
  {
    return Steinberg::kNotImplemented;
  }
  Steinberg::tresult getRoutingInfo(Steinberg::Vst::RoutingInfo& inInfo, Steinberg::Vst::RoutingInfo& outInfo) final override
  {
    return Steinberg::kNotImplemented;
  }
};
}
