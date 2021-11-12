#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cstdio>

namespace examples
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

  struct ins
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

      static consteval auto control()
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

      static consteval auto control()
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
    } audio;
  } outputs;

  void operator()(int N) { }


  static void layout(auto ui)
  {
    auto ctl1 = ui.create_control(&ins::int_ctl);
    auto ctl2 = ui.create_control(&ins::float_ctl);

  }
};
}
