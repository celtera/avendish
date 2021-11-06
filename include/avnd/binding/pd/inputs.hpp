#pragma once
#include <avnd/binding/pd/helpers.hpp>

namespace pd
{

template <typename T>
struct inputs
{
  void init(T& implementation, t_object& x_obj)
  {
    int k = 0;
    avnd::for_all_inputs(
        implementation,
        [&x_obj, &k](auto& ctl)
        {
          if (k++)
          { // Skip the first port
            if_possible(floatinlet_new(&x_obj, &ctl.value))

            // TODO
            //else if_possible(symbolinlet_new(&x_obj, &ctl.value));
            // => we must allocate a t_symbol* and copy the s_name before execution
          }
        });
  }
};

}
