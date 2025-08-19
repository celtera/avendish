#pragma once
#include <avnd/binding/vst3/controller_base.hpp>
#include <avnd/binding/vst3/programs.hpp>
#include <avnd/binding/vst3/refcount.hpp>
#include <avnd/common/widechar.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/control_display.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/controls_fp.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <cmath>
#include <pluginterfaces/vst/ivstmidicontrollers.h>

#include <cstdio>

#if defined(AVND_ADD_OSCQUERY_BINDINGS)
#include <avnd/binding/standalone/oscquery_minimal_mapper.hpp>
#endif
namespace stv3
{
template <typename T>
class Controller final
    : public stv3::ControllerCommon
    , public stv3::UnitInfo
    , public Steinberg::Vst::IMidiMapping
    , public stv3::refcount
{
  using TChar = Steinberg::Vst::TChar;
  using String128 = Steinberg::Vst::String128;
  using IBStream = Steinberg::IBStream;
  using IBStreamer = Steinberg::IBStreamer;
  using ParamValue = Steinberg::Vst::ParamValue;
  using ParameterInfo = Steinberg::Vst::ParameterInfo;
  using ParamID = Steinberg::Vst::ParamID;
  using CtrlNumber = Steinberg::Vst::CtrlNumber;

  Steinberg::tresult queryInterface(const Steinberg::TUID iid, void** obj) override
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    QUERY_INTERFACE(iid, obj, IEditController::iid, IEditController);
    QUERY_INTERFACE(iid, obj, IEditController2::iid, IEditController2);
    QUERY_INTERFACE(iid, obj, IPluginBase::iid, IPluginBase);
    QUERY_INTERFACE(iid, obj, IDependent::iid, IDependent);
    QUERY_INTERFACE(iid, obj, IConnectionPoint::iid, IConnectionPoint);
    QUERY_INTERFACE(iid, obj, IMidiMapping::iid, IMidiMapping);
    QUERY_INTERFACE(iid, obj, IUnitInfo::iid, IUnitInfo);
    *obj = nullptr;
    return Steinberg::kNoInterface;
  }

  uint32 addRef() override { return ++m_refcount; }
  uint32 release() override
  {
    if(--m_refcount == 0)
    {
      delete this;
      return 0;
    }
    return m_refcount;
  }

  using inputs_t = typename avnd::inputs_type<T>::type;
  inputs_t inputs_mirror{};
#undef AVND_ADD_OSCQUERY_BINDINGS
#if AVND_ADD_OSCQUERY_BINDINGS
  standalone::oscquery_minimal_mapper<T> oscquery_mapper{
      std::string(avnd::get_name<T>()), 1234, 5678};
#endif

  using inputs_info_t = avnd::parameter_input_introspection<T>;
  static const constexpr int32_t parameter_count = inputs_info_t::size;

public:
  Controller()
  {
#if AVND_ADD_OSCQUERY_BINDINGS
    oscquery_mapper.create_ports(inputs_mirror);
#endif
  }

  virtual ~Controller();

  int32 getParameterCount() override { return inputs_info_t::size; }

  Steinberg::tresult getParameterInfo(int32 paramIndex, ParameterInfo& info) override
  {
    if(paramIndex < 0 || paramIndex >= inputs_info_t::size)
      return Steinberg::kInvalidArgument;

    info.id = inputs_info_t::index_map[paramIndex];
    inputs_info_t::for_nth_raw(
        info.id,
        [&]<std::size_t Index, typename C>(avnd::field_reflection<Index, C> field) {
      setStr(info.title, C::name());
      setStr(info.shortTitle, C::name());
      if constexpr(requires { C::units(); })
        setStr(info.shortTitle, C::units());
      if constexpr(avnd::has_range<C>)
      {
        static constexpr auto range = avnd::get_range<C>();
        if constexpr(requires { range.init; })
          info.defaultNormalizedValue = avnd::map_control_to_01<C>(range.init);

        if constexpr(requires { range.step; })
          info.stepCount = avnd::get_range<C>().step;
      }
        });

    info.unitId = 1;
    info.flags = ParameterInfo::kCanAutomate;

    return Steinberg::kResultTrue;
  }

  ParamValue normalizedParamToPlain(ParamID tag, ParamValue valueNormalized) override
  {
    ParamValue res = valueNormalized;

    if constexpr(avnd::has_inputs<T>)
    {
      inputs_info_t::for_nth_raw(this->inputs_mirror, tag, [&]<typename C>(C& field) {
        assign_if_assignable(res, avnd::map_control_from_01_to_fp<C>(valueNormalized));
      });
    }
    return res;
  }

  ParamValue plainParamToNormalized(ParamID tag, ParamValue plainValue) override
  {
    ParamValue res = plainValue;

    if constexpr(avnd::has_inputs<T>)
    {
      inputs_info_t::for_nth_raw(this->inputs_mirror, tag, [&]<typename C>(C& field) {
        assign_if_assignable(res, avnd::map_control_from_fp_to_01<C>(plainValue));
      });
    }
    return res;
  }

  ParamValue getParamNormalized(ParamID tag) override
  {
    ParamValue res{};

    if constexpr(avnd::has_inputs<T>)
    {
      inputs_info_t::for_nth_raw(this->inputs_mirror, tag, [&]<typename C>(C& field) {
        assign_if_assignable(res, avnd::map_control_to_01(field));
      });
    }
    return res;
  }

  Steinberg::tresult setParamNormalized(ParamID tag, ParamValue value) override
  {
    if(tag < 0 || tag >= inputs_info_t::size)
      return Steinberg::kInvalidArgument;

    if constexpr(avnd::has_inputs<T>)
    {
      inputs_info_t::for_nth_raw(this->inputs_mirror, tag, [&]<typename C>(C& field) {
        assign_if_assignable(field.value, avnd::map_control_from_01<C>(value));
      });
    }

    return Steinberg::kResultTrue;
  }

  void update(FUnknown* changedUnknown, int32 message) override { }

  Steinberg::tresult setComponentState(IBStream* state) override
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    if(!state)
      return Steinberg::kResultFalse;

    IBStreamer streamer(state, kLittleEndian);

    if constexpr(avnd::has_inputs<T>)
    {
      bool ok = inputs_info_t::for_all_unless(
          this->inputs_mirror, [&]<typename C>(C& field) -> bool {
            double param = 0.f;
            if(streamer.readDouble(param) == false)
              return false;

            assign_if_assignable(field.value, avnd::map_control_from_01<C>(param));

            return true;
          });

      return ok ? Steinberg::kResultOk : Steinberg::kResultFalse;
    }
    else
    {
      return Steinberg::kResultOk;
    }
  }

  Steinberg::tresult setState(IBStream* state) override
  {
    IBStreamer streamer(state, kLittleEndian);

    int8 byteOrder;
    if(streamer.readInt8(byteOrder) == false)
      return Steinberg::kResultFalse;

    return Steinberg::kResultTrue;
  }

  Steinberg::tresult getState(IBStream* state) override
  {
    IBStreamer streamer(state, kLittleEndian);

    int8 byteOrder = BYTEORDER;
    if(streamer.writeInt8(byteOrder) == false)
      return Steinberg::kResultFalse;

    return Steinberg::kResultTrue;
  }

  Steinberg::tresult getParamStringByValue(
      ParamID tag, ParamValue valueNormalized, String128 string) override
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    bool ok = false;
    inputs_info_t::for_nth_raw(
        tag, [&]<auto Idx, typename C>(avnd::field_reflection<Idx, C> tag) {
          if constexpr(requires { avnd::map_control_from_01<C>(valueNormalized); })
          {
            ok = avnd::display_control<C>(
                avnd::map_control_from_01<C>(valueNormalized), string, 128);
          }
        });

    return ok ? Steinberg::kResultTrue : Steinberg::kResultFalse;
  }

  Steinberg::tresult
  getParamValueByString(ParamID tag, TChar* string, ParamValue& valueNormalized) override
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    return Steinberg::kResultFalse;
  }

  Steinberg::tresult getMidiControllerAssignment(
      int32 busIndex, int16 channel, CtrlNumber midiControllerNumber,
      ParamID& tag) override
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    return Steinberg::kResultFalse;
  }
};

template <typename T>
Controller<T>::~Controller()
{
}
}
