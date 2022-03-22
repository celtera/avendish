#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/helpers/polyfill.hpp>
#include <avnd/helpers/static_string.hpp>
#include <string_view>

namespace avnd
{

template<int w>
struct hspace {
    enum { spacing };    
    static constexpr auto width() { return w; }
    static constexpr auto height() { return 1; }
};
template<int h>
struct vspace {
    enum { spacing };    
    static constexpr auto width() { return 1; }
    static constexpr auto height() { return h; }
};
template<int w, int h>
struct space {
    enum { spacing };    
    static constexpr auto width() { return w; }
    static constexpr auto height() { return h; }
};
/*
struct hbox {
    enum { hbox };
};
struct vbox {
    enum { vbox };    
};

template <static_string lit>
struct group {
    enum { group };    
    static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
};
struct tabs {
    enum { tabs };    
};
template<int w, int h>
struct split {
    enum { split };    
    static constexpr auto width() { return w; }
    static constexpr auto height() { return h; }
};
template<static_string lit, typename Layout>
struct tab : Layout {
    static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
};*/

#define avnd_cat2(a, b) a ## b
#define avnd_cat(a, b) avnd_cat2(a, b)
#define avnd_lay struct :

#define avnd_hbox struct { enum { hbox };  
#define avnd_vbox struct { enum { vbox };  
#define avnd_tabs struct { enum { tabs };  
#define avnd_split(w, h) struct { enum { split }; static constexpr auto width() { return w; } static constexpr auto height() { return h; }

#define avnd_group(Name) struct { enum { group };  static clang_buggy_consteval auto name() { return std::string_view{Name};  }
#define avnd_tab(Name, Type) struct { enum { Type };  static clang_buggy_consteval auto name() { return std::string_view{Name};  }

#define avnd_close } avnd_cat(widget_, __LINE__)

#define avnd_widget(Inputs, Ctl) decltype(&Inputs::Ctl) Ctl = &Inputs::Ctl; 
}
