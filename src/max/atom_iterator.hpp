#pragma once
#include <common/coroutines.hpp>
#include <ext.h>

#include <variant>

namespace max
{
using atom_iterator = avnd::generator<std::variant<double, std::string_view>>;

inline atom_iterator make_atom_iterator(int argc, t_atom* argv)
{
  for (int i = 0; i < argc; ++i)
  {
    switch (argv[i].a_type)
    {
      case A_FLOAT:
      {
        co_yield argv[i].a_w.w_float;
        break;
      }
      case A_SYM:
      {
        co_yield std::string_view{argv[i].a_w.w_sym->s_name};
        break;
      }
      default:
      {
        break;
      }
    }
  }
}

}
