#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/metadatas.hpp>
#include <avnd/binding/vst3/metadata.hpp>
#include <avnd/common/limited_string_view.hpp>
#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>

namespace stv3
{
using factory_vendor = avnd::limited_string_view<Steinberg::PFactoryInfo::kNameSize>;
using factory_url = avnd::limited_string_view<Steinberg::PFactoryInfo::kURLSize>;
using factory_email = avnd::limited_string_view<Steinberg::PFactoryInfo::kEmailSize>;

using class_name = avnd::limited_string_view<Steinberg::PClassInfo::kNameSize>;
using class_category = avnd::limited_string_view<Steinberg::PClassInfo::kCategorySize>;
using class_subcategories
    = avnd::limited_string_view<Steinberg::PClassInfo2::kSubCategoriesSize>;
using class_vendor = avnd::limited_string_view<Steinberg::PClassInfo2::kVendorSize>;
using class_version = avnd::limited_string_view<Steinberg::PClassInfo2::kVersionSize>;

template <typename T, typename Component, typename Controller>
class factory : public Steinberg::IPluginFactory3
{

  static constexpr auto component_uid = stv3::component_uuid_for_type<T>();
  static constexpr auto controller_uid = stv3::controller_uuid_for_type<T>();

public:
  Steinberg::tresult queryInterface(const Steinberg::TUID iid, void** obj) override
  {
    QUERY_INTERFACE(iid, obj, Steinberg::IPluginFactory::iid, Steinberg::IPluginFactory)
    QUERY_INTERFACE(
        iid, obj, Steinberg::IPluginFactory2::iid, Steinberg::IPluginFactory2)
    QUERY_INTERFACE(
        iid, obj, Steinberg::IPluginFactory3::iid, Steinberg::IPluginFactory3)
    QUERY_INTERFACE(iid, obj, Steinberg::FUnknown::iid, Steinberg::IPluginFactory)
    *obj = nullptr;
    return Steinberg::kNoInterface;
  }

  uint32 addRef() override { return 1; }
  uint32 release() override { return 1; }

  Steinberg::tresult getFactoryInfo(Steinberg::PFactoryInfo* info) override
  {
    if constexpr (requires { T::vendor(); })
      factory_vendor{avnd::get_vendor<T>()}.copy_to(info->vendor);
    else
      factory_vendor{""}.copy_to(info->vendor);

    if constexpr (requires { T::url(); })
      factory_url{avnd::get_url<T>()}.copy_to(info->url);
    else
      factory_url{""}.copy_to(info->url);

    if constexpr (requires { T::email(); })
      factory_email{avnd::get_email<T>()}.copy_to(info->email);
    else
      factory_email{""}.copy_to(info->email);

    info->flags = Steinberg::Vst::kDefaultFactoryFlags;

    return Steinberg::kResultOk;
  }

  int32 countClasses() override { return 2; }

  Steinberg::tresult getClassInfo(int32 index, Steinberg::PClassInfo* info) override
  {
    memset(info, 0, sizeof(Steinberg::PClassInfo));
    switch (index)
    {
      case 0:
      {
        memcpy(info->cid, &component_uid, sizeof(Steinberg::TUID));
        info->cardinality = Steinberg::PClassInfo::kManyInstances;
        class_category{kVstAudioEffectClass}.copy_to(info->category);
        class_name{avnd::get_name<T>()}.copy_to(info->name);
        return Steinberg::kResultOk;
      }
      case 1:
      {
        memcpy(info->cid, &controller_uid, sizeof(Steinberg::TUID));
        info->cardinality = Steinberg::PClassInfo::kManyInstances;
        class_category{kVstComponentControllerClass}.copy_to(info->category);
        snprintf(
            info->name, Steinberg::PClassInfo::kNameSize, "%sController", avnd::get_name<T>().data());
        return Steinberg::kResultOk;
      }
      default:
        return Steinberg::kInvalidArgument;
    }
  }

  Steinberg::tresult getClassInfo2(int32 index, Steinberg::PClassInfo2* info) override
  {
    memset(info, 0, sizeof(Steinberg::PClassInfo2));
    getClassInfo(index, reinterpret_cast<Steinberg::PClassInfo*>(info));

    switch (index)
    {
      case 0:
        info->classFlags = Steinberg::Vst::kDistributable;
        class_subcategories{"Fx"}.copy_to(info->subCategories);
        break;
      case 1:
        break;
      default:
        return Steinberg::kInvalidArgument;
    }

    if constexpr (requires { T::vendor(); })
      class_vendor{T::vendor()}.copy_to(info->vendor);
    else
      class_vendor{"Undefined"}.copy_to(info->vendor);

    if constexpr (requires { T::version(); })
      class_version{T::version()}.copy_to(info->version);
    else
      class_version{"0.0.0"}.copy_to(info->version);

    class_version{kVstVersionString}.copy_to(info->sdkVersion);
    return Steinberg::kResultOk;
  }

  Steinberg::tresult
  getClassInfoUnicode(int32 index, Steinberg::PClassInfoW* info) override
  {
    memset(info, 0, sizeof(Steinberg::PClassInfoW));
    switch (index)
    {
      case 0:
      {
        memcpy(info->cid, &component_uid, sizeof(Steinberg::TUID));
        info->cardinality = Steinberg::PClassInfo::kManyInstances;
        class_category{kVstAudioEffectClass}.copy_to(info->category);
        class_name{avnd::get_name<T>()}.copy_to(info->name);

        info->classFlags = Steinberg::Vst::kDistributable;
        class_subcategories{"Fx"}.copy_to(info->subCategories);
        break;
      }
      case 1:
      {
        memcpy(info->cid, &controller_uid, sizeof(Steinberg::TUID));
        info->cardinality = Steinberg::PClassInfo::kManyInstances;
        class_category{kVstComponentControllerClass}.copy_to(info->category);
        class_name{avnd::get_name<T>()}.copy_to(info->name);
        constexpr int name_len = avnd::get_name<T>().size();
        avnd::limited_string_view<Steinberg::PClassInfo::kNameSize - name_len>{
            "Controller"}
            .copy_to(info->name + name_len);
        break;
      }
      default:
        return Steinberg::kInvalidArgument;
    }

    if constexpr (requires { T::vendor(); })
      class_vendor{avnd::get_vendor<T>()}.copy_to(info->vendor);
    else
      class_vendor{""}.copy_to(info->vendor);

    if constexpr (requires { T::version(); })
      class_version{avnd::get_version<T>()}.copy_to(info->version);
    else
      class_version{"0.0.0"}.copy_to(info->version);

    class_version{kVstVersionString}.copy_to(info->sdkVersion);
    return Steinberg::kResultOk;
  }

  Steinberg::tresult createInstance(
      Steinberg::FIDString cid,
      Steinberg::FIDString _iid,
      void** obj) override
  {
    using namespace Steinberg::Vst;
    if (memcmp(&component_uid, cid, sizeof(Steinberg::TUID)) == 0)
    {
      FUnknown* instance = static_cast<IAudioProcessor*>(new Component);
      if (instance->queryInterface(_iid, obj) == Steinberg::kResultOk)
      {
        instance->release();
        return Steinberg::kResultOk;
      }
      else
      {
        instance->release();
      }
    }
    else if (memcmp(&controller_uid, cid, sizeof(Steinberg::TUID)) == 0)
    {
      FUnknown* instance = static_cast<IEditController*>(new Controller);
      if (instance->queryInterface(_iid, obj) == Steinberg::kResultOk)
      {
        instance->release();
        return Steinberg::kResultOk;
      }
      else
      {
        instance->release();
      }
    }
    *obj = nullptr;
    return Steinberg::kNoInterface;
  }

  Steinberg::tresult setHostContext(FUnknown* context) override
  {
    return Steinberg::kResultOk;
  }
};
}
