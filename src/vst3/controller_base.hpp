#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <vst3/metadata.hpp>
#include <vst3/connection_point.hpp>

#include <base/source/fstreamer.h>
#include <base/source/fstring.h>
#include <base/source/updatehandler.h>
#include <pluginterfaces/base/ibstream.h>
#include <pluginterfaces/base/ustring.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
namespace stv3
{

static inline Steinberg::Vst::KnobMode hostKnobMode = Steinberg::Vst::kCircularMode;

struct ControllerCommon
    : public Steinberg::IDependent
    , public stv3::ConnectionPoint
    , public Steinberg::Vst::IEditController
    , public Steinberg::Vst::IEditController2
{
public:
  ControllerCommon()
  {
    Steinberg::UpdateHandler::instance();
  }

  Steinberg::tresult initialize(Steinberg::FUnknown* context) final override
  {
    {
      // check if already initialized
      if (hostContext)
        return Steinberg::kResultFalse;

      hostContext = context;
    }

    return Steinberg::kResultTrue;
  }

  Steinberg::tresult terminate() final override
  {
    if (componentHandler)
    {
      componentHandler->release();
      componentHandler = nullptr;
    }

    if (componentHandler2)
    {
      componentHandler2->release();
      componentHandler2 = nullptr;
    }

    // release host interfaces
    hostContext = nullptr;

    // in case host did not disconnect us,
    // release peer now
    if (peerConnection)
    {
      peerConnection->disconnect(this);
      peerConnection = nullptr;
    }

    return Steinberg::kResultOk;
  }

  Steinberg::tresult setComponentHandler(Steinberg::Vst::IComponentHandler* newHandler) final override
  {
    if (componentHandler == newHandler)
    {
      return Steinberg::kResultTrue;
    }

    if (componentHandler)
    {
      componentHandler->release();
    }

    componentHandler = newHandler;
    if (componentHandler)
    {
      componentHandler->addRef();
    }

    // try to get the extended version
    if (componentHandler2)
    {
      componentHandler2->release();
      componentHandler2 = nullptr;
    }

    if (newHandler)
    {
      newHandler->queryInterface(
          Steinberg::Vst::IComponentHandler2::iid, (void**)&componentHandler2);
    }
    return Steinberg::kResultTrue;
  }

  Steinberg::IPlugView* createView(const char* name) override
  {
    return nullptr;
  }

  Steinberg::tresult setKnobMode(Steinberg::Vst::KnobMode mode) override
  {
    hostKnobMode = mode;
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult openHelp(Steinberg::TBool /*onlyCheck*/) override
  {
    return Steinberg::kResultFalse;
  }

  Steinberg::tresult openAboutBox(Steinberg::TBool /*onlyCheck*/) override
  {
    return Steinberg::kResultFalse;
  }

  Steinberg::IPtr<FUnknown> hostContext;
  Steinberg::Vst::IComponentHandler* componentHandler{};
  Steinberg::Vst::IComponentHandler2* componentHandler2{};
};
}
