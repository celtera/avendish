#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <string_view>

#if defined(_WIN32)
#define HALP_HIDDEN_SYMBOL
#else
#define HALP_HIDDEN_SYMBOL __attribute__((visibility("hidden")))
#endif

#define HALP_TOKENPASTE(x, y) x##y
#define HALP_TOKENPASTE2(x, y) HALP_TOKENPASTE(x, y)

namespace halp
{

/// Refers to a member or free function ///

template <static_string lit, auto M>
struct func_ref
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
  static clang_buggy_consteval auto func() { return M; }
};

}

#define halp_start_messages(T)       \
  struct HALP_HIDDEN_SYMBOL messages \
  {                                  \
    using parent_type = T;
#define halp_end_messages \
  }                       \
  ;

#define halp_mem_fun(Mem) ::halp::func_ref<#Mem, &parent_type::Mem> m_##Mem;
#define halp_mem_fun_t(Mem, MemT)                                \
  ::halp::func_ref<#Mem, &parent_type::Mem MemT> HALP_TOKENPASTE2( \
      m_, HALP_TOKENPASTE2(Mem, __LINE__));
#define halp_free_fun(Fun) ::halp::func_ref<#Fun, Fun> m_##Fun;
#define halp_free_fun_t(Fun, FunT) \
  ::halp::func_ref<#Fun, Fun FunT> HALP_TOKENPASTE2(m_, HALP_TOKENPASTE2(Mem, __LINE__));

#if (defined(__clang__) || defined(_MSC_VER))
#define halp_lambda(Name, Fun) \
  struct                       \
  {                            \
    halp_meta(name, #Name)     \
    halp_meta(func, Fun)       \
  } m_##Name;
#else
#define halp_lambda(Name, Fun)
#endif
