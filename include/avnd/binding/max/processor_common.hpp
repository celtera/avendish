#pragma once
#include <avnd/binding/max/attributes_setup.hpp>
#include <avnd/binding/max/inputs.hpp>
#include <avnd/binding/max/outputs.hpp>
#include <avnd/binding/max/init.hpp>
#include <avnd/binding/max/helpers.hpp>
#include <avnd/binding/max/dict.hpp>
#include <avnd/binding/max/from_dict.hpp>
#include <avnd/binding/max/messages.hpp>


namespace max
{

template<typename T>
struct processor_common
{
  static void get_inlet_description(long index, char *dst)
  {
    avnd::input_introspection<T>::for_nth(index, [dst] <typename Field> (const Field& port) {
      if constexpr(avnd::has_description<typename Field::type>) {
        auto str = avnd::get_description<typename Field::type>();
        strcpy(dst, str.data());
      }
    });
  }

  static void get_outlet_description(long index, char *dst)
  {
    avnd::output_introspection<T>::for_nth(index, [dst] <typename Field> (const Field& port) {
      if constexpr(avnd::has_description<typename Field::type>) {
        auto str = avnd::get_description<typename Field::type>();
        strcpy(dst, str.data());
      }
    });
  }

  static void obj_assist(processor_common* obj, void *b, long msg, long arg, char *dst) {
    switch(msg) {
      case 1:
        processor_common::get_inlet_description(arg, dst);
        break;
      default:
        processor_common::get_outlet_description(arg, dst);
        break;
    }
  };
};
}
