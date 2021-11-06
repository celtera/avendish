#pragma once
#include <avnd/common/coroutines.hpp>
#include <m_pd.h>

#include <variant>

namespace pd
{
using atom_iterator = avnd::generator<std::variant<float, std::string_view>>;

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
      case A_SYMBOL:
      {
        co_yield std::string_view{argv[i].a_w.w_symbol->s_name};
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
