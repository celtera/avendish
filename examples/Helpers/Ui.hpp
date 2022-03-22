#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/layout.hpp>
#include <avnd/helpers/meta.hpp>
#include <avnd/concepts/processor.hpp>
#include <cmath>
#include <cstdio>

template<typename M, typename L, typename T>
struct prop { 
    std::function<T(M& self, L& layout)> get;
    std::function<void(M& self, L& layout, const T&)> set;
};
// template<typename T>
// using prop = ::prop<Ui, layout, T>;

namespace examples::helpers
{
/**
 * Example to test UI.
 */
struct Ui
{
  static consteval auto name() { return "UI example"; }
  static consteval auto c_name() { return "avnd_ui"; }
  static consteval auto uuid()
  {
    return "e1f0f202-6732-4d2d-8ee9-5957a51ae667";
  }

  struct inputs_t
  {
    struct
    {
      static consteval auto name() { return "Input"; }
      double** samples;
      int channels;
    } audio;

    struct
    {
      static consteval auto name() { return "Int control"; }

      // The reflection system will look for common widget names in here.
      // First one it finds will be used.
      enum widget
      {
        slider
      };

      static consteval auto range()
      {
        struct
        {
          const int min = 0;
          const int max = 10;
          const int init = 3;
        } c;
        return c;
      }

      static void display(char* buf, int v) noexcept
      {
        snprintf(buf, 16, "%d dabloons", v);
      }

      int value;
    } int_ctl;

    struct
    {
      static consteval auto name() { return "Float control"; }

      enum widget
      {
        knob
      };

      static consteval auto range()
      {
        struct
        {
          const float min = 0;
          const float max = 10;
          const float init = 3;
        } c;
        return c;
      }

      float value;
    } float_ctl;
  } inputs;

  struct
  {
    struct
    {
      static consteval auto name() { return "Output"; }
      double** samples;
      int channels;
    } audio;
  } outputs;

  void operator()(int N) { }
  
  struct layout {
      enum { vbox };    
      avnd_hbox
        avnd_widget(inputs_t, float_ctl);
      avnd_close;
      
      avnd::hspace<20> a_spacing;      
      
      avnd_group("Group")
        avnd_hbox
          const char* l1 = "label 1";
          const char* l2 = "label 2";
        avnd_close;
      avnd_close;
      
      avnd::hspace<20> b_spacing;
      
      avnd_tabs
        avnd_tab("HBox", hbox)
          const char* l1 = "label 3";
          const char* l2 = "label 4";
          avnd_widget(inputs_t, int_ctl);
        avnd_close;
          
        avnd_tab("VBox", vbox) 
          const char* l1 = "label 5";
          const char* l2 = "label 6";
        avnd_close;
      avnd_close;
  
      avnd_split(400, 200)
        const char* l1 = "some long foo";
        const char* l2 = "other bar";
      avnd_close;
    avnd_close;
};
}
