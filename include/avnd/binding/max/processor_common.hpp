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
  void init(this auto& self, int argc, char** argv) {

  }

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


  void process_inlet_control(avnd::effect_container<T>& implementation, inputs<T>& input_setup, int inlet, auto val)
  {
    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      auto& obj = implementation.effect;
      input_setup.for_inlet(inlet, implementation, [&obj, val] <typename F>(F& field) {
        if constexpr(convertible_to_atom_list_statically<decltype(field.value)>)
        {
          t_atom res;
          value_to_max(res, val);
          if(from_atoms{1, &res}(field.value))
          {
            if_possible(field.update(obj));
            return true;
          }
          else
          {
            return false;
          }
        }
      });
    }
  }

  void process_inlet_dict(avnd::effect_container<T>& implementation, inputs<T>& input_setup, int inlet, struct symbol* val)
  {
    if constexpr(max::dict_parameter_inputs_introspection<T>::size > 0)
    {
      auto dict = dictobj_findregistered_retain(val);
      if(!dict)
        return;

      auto& obj = implementation.effect;
      input_setup.for_inlet(inlet, implementation, [&obj, dict] <typename F>(F& field) {
        if constexpr(max::dict_parameter<F>)
        {
          from_dict{}(dict, field.value);
          if_possible(field.update(obj));
        }
      });
      dictobj_release(dict);
    }
  }

  template <typename Field>
  static bool
  process_inlet_control(T& obj, Field& field, int argc, t_atom* argv)
  {
    if constexpr(convertible_to_atom_list_statically<decltype(field.value)>)
    {
      if(from_atoms{argc, argv}(field.value))
      {
        if_possible(field.update(obj));
        return true;
      }
    }
    else
    {
      // FIXME
    }
    return false;
  }

  void process_inlet_control(avnd::effect_container<T>& implementation, inputs<T>& input_setup, int inlet, t_symbol* s, int argc, t_atom* argv)
  {
    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      auto& obj = implementation.effect;
      input_setup.for_inlet(inlet, implementation, [&obj, argc, argv] <typename F>(F& field) {
        process_inlet_control(obj, field, argc, argv);
      });
    }
  }

  bool process_inputs(
      avnd::effect_container<T>& implementation, t_symbol* s, int argc, t_atom* argv)
  {
    // FIXME create static pointer tables instead, implement for_each_field_function_table
    // FIXME refactor with pd too
    if constexpr(avnd::parameter_input_introspection<T>::size > 0)
    {
      bool ok = false;
      std::string_view symname = s->s_name;
      avnd::parameter_input_introspection<T>::for_all(
          avnd::get_inputs(implementation), [&]<typename M>(M& field) {
        if(ok)
          return;
        if(symname == max::get_name_symbol<M>())
        {
          ok = process_inlet_control(implementation.effect, field, argc, argv);
        }
      });
      return ok;
    }
    return false;
  }

};
}
