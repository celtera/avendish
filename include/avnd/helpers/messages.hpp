#pragma once
#include <avnd/helpers/static_string.hpp>
#include <string_view>

namespace avnd
{

/// Refers to a member or free function ///

template <static_string lit, auto M>
struct func_ref
{
  static consteval auto name() { return std::string_view{lit.value}; }
  static consteval auto func() { return M; }
};

}

#define avnd_start_messages(T) struct messages { using parent_type = T;
#define avnd_end_messages };

#define avnd_mem_fun(Mem) avnd::func_ref<#Mem, &parent_type::Mem> m_ ## Mem;
#define avnd_free_fun(Fun) avnd::func_ref<#Fun, Fun>  m_ ## Fun;

// broken in GCC: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83258
#if (defined(__clang__) || defined(_MSC_VER))
  #define avnd_lambda(Name, Fun)                         \
    struct {                                             \
      $(name, #Name)                                     \
      $(func, +Fun)                                      \
    } m_ ## Name;
#else
  #define avnd_lambda(Name, Fun)
#endif
