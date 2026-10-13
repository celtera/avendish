#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/controls.hpp>
#include <avnd/wrappers/process_adapter.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/common/export.hpp>
#include <avnd/helpers/callback.hpp>


#include <mapper/mapper_cpp.h>


#include <span>
#include <cmath>
#include <string>

namespace libmapper
{
template <typename T>
struct processor
{
  T& t;
  mapper::Device dev;


  static constexpr const int input_controls = avnd::parameter_input_introspection<T>::size;
  static constexpr const int output_controls = avnd::parameter_output_introspection<T>::size;

  std::array<mapper::Signal, input_controls> input_signals;
  std::array<mapper::Signal, output_controls> output_signals;

  template <auto Idx, typename C>
  static std::string input_name(avnd::field_reflection<Idx, C>)
  {
    if constexpr (requires { C::name(); })
      return "/" + std::string(C::name());
    else
      return "/in." + std::to_string(Idx);
  }

  template <auto Idx, typename C>
  static std::string output_name(avnd::field_reflection<Idx, C>)
  {
    if constexpr (requires { C::name(); })
      return "/" + std::string(C::name());
    else
      return "/out." + std::to_string(Idx);
  }

  template <auto Idx, typename C>
  void setup_input(avnd::field_reflection<Idx, C> refl)
  {
    using namespace std::literals;
    {
      const std::string param_name = input_name(refl);
      if constexpr(avnd::float_parameter<C>)
      {
          std::cerr << param_name << std::endl;
        static auto callback = [] (mapper::Signal&& sig, float value, mapper::Time&& time) {
          T& obj = *(T*)(void*)sig["object"];
          boost::pfr::get<Idx>(obj.inputs).value = value;
        };
        if constexpr(requires { C::control(); }) 
        {
          static const auto ctl = C::control();
          input_signals[Idx] = dev.add_signal(
            mapper::Direction::INCOMING, param_name, 1, 
            mapper::Type::FLOAT, "", (void*)&ctl.min, (void*)&ctl.max, nullptr)
            .set_property("object", (void*)&t)
            .set_callback(callback, mapper::Signal::Event::UPDATE);
        }
        else
        {
          static float min0 = 0.f;
          static float max1 = 1.f;
          input_signals[Idx] = dev.add_signal(
            mapper::Direction::INCOMING, param_name, 1, 
            mapper::Type::FLOAT, "", (void*)&min0, (void*)&max1, nullptr)
            .set_property("object", (void*)&t)
            .set_callback(callback, mapper::Signal::Event::UPDATE);
        }
      }
      else if constexpr(avnd::int_parameter<C>)
      {
        static auto callback = [] (mapper::Signal&& sig, int value, mapper::Time&& time) {
          T& obj = *(T*)(void*)sig["object"];
          boost::pfr::get<Idx>(obj.inputs).value = value;
        };
        if constexpr(requires { C::control(); }) 
        {
          static const auto ctl = C::control();
          input_signals[Idx] = dev.add_signal(
            mapper::Direction::INCOMING, param_name, 1, 
            mapper::Type::INT32, "", (void*)&ctl.min, (void*)&ctl.max, nullptr)
            .set_property("object", (void*)&t)
            .set_callback(callback, mapper::Signal::Event::UPDATE);
        }
      }
    }
  }

  template <auto Idx, typename C>
  void setup_output(avnd::field_reflection<Idx, C> refl)
  {
    using namespace std::literals;
    {
      const std::string param_name = output_name(refl);

      if constexpr(avnd::float_parameter<C>)
      {
        if constexpr(requires { C::control(); })
        {
          static const auto ctl = C::control();
          output_signals[Idx] = dev.add_signal(
            mapper::Direction::OUTGOING, param_name, 1,
            mapper::Type::FLOAT, "", (void*)&ctl.min, (void*)&ctl.max, nullptr);
        }
        else
        {
          static float min0 = 0.f;
          static float max1 = 1.f;
          output_signals[Idx] = dev.add_signal(
            mapper::Direction::OUTGOING, param_name, 1,
            mapper::Type::FLOAT, "", (void*)&min0, (void*)&max1, nullptr);
        }
      }
      else if constexpr(avnd::int_parameter<C>)
      {
        if constexpr(requires { C::control(); })
        {
          static const auto ctl = C::control();
          output_signals[Idx] = dev.add_signal(
            mapper::Direction::OUTGOING, param_name, 1,
            mapper::Type::INT32, "", (void*)&ctl.min, (void*)&ctl.max, nullptr);
        }
      }
    }
  }

  explicit processor(T& t)
  : t{t}
  , dev{T::name()}
  {
    if constexpr (avnd::inputs_is_value<T>)
    {
      avnd::parameter_input_introspection<T>::for_all([this](auto a)
                                                      { setup_input(a); });
    }

    if constexpr (avnd::outputs_is_value<T>)
    {
      avnd::parameter_output_introspection<T>::for_all([this](auto a)
                                                       { setup_output(a); });
    }
    fflush(stdout);
    fflush(stderr);
  }

  void poll()
  {
    dev.poll(10);
  }

  void commit()
  {
    if constexpr (avnd::outputs_is_value<T>)
    {
      int k = 0;
      avnd::parameter_output_introspection<T>::for_all(
                  t.outputs,
                  [this, &k] (auto& c) {
          mapper::Signal& sig = this->output_signals[k++];
          sig.set_value(c.value);
      });
    }
  }
};
}
