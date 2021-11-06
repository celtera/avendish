#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <vst3/helpers.hpp>
#include <pluginterfaces/vst/ivstunits.h>

namespace stv3
{

class UnitInfo : public Steinberg::Vst::IUnitInfo
{
  Steinberg::Vst::UnitID selectedUnit{Steinberg::Vst::kRootUnitId};

  int32 getUnitCount() override { return 1; }

  Steinberg::tresult getUnitInfo(int32 unitIndex, Steinberg::Vst::UnitInfo& info /*out*/) override
  {
    info.id = 1;
    info.parentUnitId = Steinberg::Vst::kRootUnitId;
    setStr(info.name, u"Unit1");
    info.programListId = Steinberg::Vst::kNoProgramListId;
    return Steinberg::kResultTrue;
  }

  int32 getProgramListCount() override
  {
    return 0;
    //return static_cast<int32>(programLists.size());
  }

  Steinberg::tresult
  getProgramListInfo(int32 listIndex, Steinberg::Vst::ProgramListInfo& info /*out*/) override
  {
    if(listIndex < 0)
      return Steinberg::kResultFalse;
      /*
    if (listIndex < 0 || listIndex >= static_cast<int32>(programLists.size()))
      return Steinberg::kResultFalse;
    info = programLists[listIndex]->getInfo();
    */
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult getProgramName(
      Steinberg::Vst::ProgramListID listId,
      int32 programIndex,
      Steinberg::Vst::String128 name /*out*/) override
  {
    /*
    ProgramIndexMap::const_iterator it = programIndexMap.find(listId);
    if (it != programIndexMap.end())
    {
      return programLists[it->second]->getProgramName(programIndex, name);
    }
    */
    return Steinberg::kResultFalse;
  }

  Steinberg::tresult getProgramInfo(
      Steinberg::Vst::ProgramListID listId,
      int32 programIndex,
      Steinberg::Vst::CString attributeId /*in*/,
      Steinberg::Vst::String128 attributeValue /*out*/) override
  {
    /*
    ProgramIndexMap::const_iterator it = programIndexMap.find(listId);
    if (it != programIndexMap.end())
    {
      return programLists[it->second]->getProgramInfo(
          programIndex, attributeId, attributeValue);
    }
    */
    return Steinberg::kResultFalse;
  }

  Steinberg::tresult
  hasProgramPitchNames(Steinberg::Vst::ProgramListID listId, int32 programIndex) override
  {
    /*
    ProgramIndexMap::const_iterator it = programIndexMap.find(listId);
    if (it != programIndexMap.end())
    {
      return programLists[it->second]->hasPitchNames(programIndex);
    }
    */
    return Steinberg::kResultFalse;
  }
  Steinberg::tresult getProgramPitchName(
      Steinberg::Vst::ProgramListID listId,
      int32 programIndex,
      int16 midiPitch,
      Steinberg::Vst::String128 name /*out*/) override
  {
    /*
    ProgramIndexMap::const_iterator it = programIndexMap.find(listId);
    if (it != programIndexMap.end())
    {
      return programLists[it->second]->getPitchName(
          programIndex, midiPitch, name);
    }
    */
    return Steinberg::kResultFalse;
  }

  Steinberg::Vst::UnitID getSelectedUnit() override { return selectedUnit; }

  Steinberg::tresult selectUnit(Steinberg::Vst::UnitID unitId) override
  {
    selectedUnit = unitId;
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult getUnitByBus(
      Steinberg::Vst::MediaType /*type*/,
      Steinberg::Vst::BusDirection /*dir*/,
      int32 /*busIndex*/,
      int32 /*channel*/,
      Steinberg::Vst::UnitID& /*unitId*/ /*out*/) override
  {
    return Steinberg::kResultFalse;
  }
  Steinberg::tresult setUnitProgramData(
      int32 /*listOrUnitId*/,
      int32 /*programIndex*/,
      Steinberg::IBStream* /*data*/) override
  {
    return Steinberg::kResultFalse;
  }

};
}
