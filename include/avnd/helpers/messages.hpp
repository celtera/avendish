#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/helpers/polyfill.hpp>
#include <avnd/helpers/static_string.hpp>

#include <string_view>

#if defined(_WIN32)
#define AVND_HIDDEN_SYMBOL
#else
#define AVND_HIDDEN_SYMBOL __attribute__((visibility("hidden")))
#endif

#define AVND_TOKENPASTE(x, y) x##y
#define AVND_TOKENPASTE2(x, y) AVND_TOKENPASTE(x, y)

namespace avnd
{

/// Refers to a member or free function ///

template <static_string lit, auto M>
struct func_ref
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
  static clang_buggy_consteval auto func() { return M; }
};

}

#define avnd_start_messages(T)       \
  struct AVND_HIDDEN_SYMBOL messages \
  {                                  \
    using parent_type = T;
#define avnd_end_messages \
  }                       \
  ;

#define avnd_mem_fun(Mem) avnd::func_ref<#Mem, &parent_type::Mem> m_##Mem;
#define avnd_mem_fun_t(Mem, MemT)                                \
  avnd::func_ref<#Mem, &parent_type::Mem MemT> AVND_TOKENPASTE2( \
      m_, AVND_TOKENPASTE2(Mem, __LINE__));
#define avnd_free_fun(Fun) avnd::func_ref<#Fun, Fun> m_##Fun;
#define avnd_free_fun_t(Fun, FunT) \
  avnd::func_ref<#Fun, Fun FunT> AVND_TOKENPASTE2(m_, AVND_TOKENPASTE2(Mem, __LINE__));

#if (defined(__clang__) || defined(_MSC_VER))
#define avnd_lambda(Name, Fun) \
  struct                       \
  {                            \
    $(name, #Name)             \
    $(func, Fun)               \
  } m_##Name;
#else
#define avnd_lambda(Name, Fun)
#endif
