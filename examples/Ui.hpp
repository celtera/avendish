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
  static consteval auto uuid() { return "e1f0f202-6732-4d2d-8ee9-5957a51ae667"; }

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
      static consteval auto name() { return "Int"; }

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
      static consteval auto name() { return "Float"; }

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

  struct ui
  {
    static constexpr auto name() { return "Main"; }
    static constexpr auto layout()
    {
      enum
      {
        hbox
      } d{};
      return d;
    }
    static constexpr auto background()
    {
      enum
      {
        mid
      } d{};
      return d;
    }
    struct
    {
      static constexpr auto layout()
      {
        enum
        {
          hbox
        } d{};
        return d;
      }
      static constexpr auto background()
      {
        enum
        {
          dark
        } d{};
        return d;
      }
      decltype(&ins::int_ctl) int_widget = &ins::int_ctl;
    } widgets;

    struct
    {
      static constexpr auto layout()
      {
        enum
        {
          spacing
        } d{};
        return d;
      }
      static constexpr auto width() { return 20; }
      static constexpr auto height() { return 20; }
    } a_spacing;

    struct
    {
      static constexpr auto name() { return "Group"; }
      static constexpr auto layout()
      {
        enum
        {
          group
        } d{};
        return d;
      }
      static constexpr auto background()
      {
        enum
        {
          light
        } d{};
        return d;
      }
      struct
      {
        static constexpr auto layout()
        {
          enum
          {
            hbox
          } d{};
          return d;
        }
        const char* l1 = "label 1";
        struct
        {
          static constexpr auto layout()
          {
            enum
            {
              spacing
            } d{};
            return d;
          }
          static constexpr auto width() { return 20; }
          static constexpr auto height() { return 10; }
        } b_spacing;
        const char* l2 = "label 2";
      } a_hbox;
    } b_group;

    struct
    {
      static constexpr auto name() { return "Tabs"; }
      static constexpr auto layout()
      {
        enum
        {
          tabs
        } d{};
        return d;
      }
      static constexpr auto background()
      {
        enum
        {
          darker
        } d{};
        return d;
      }

      struct
      {
        static constexpr auto layout()
        {
          enum
          {
            hbox
          } d{};
          return d;
        }
        static constexpr auto name() { return "HBox"; }
        const char* l1 = "label 3";
        const char* l2 = "label 4";
      } a_hbox;

      struct
      {
        static constexpr auto layout()
        {
          enum
          {
            vbox
          } d{};
          return d;
        }
        static constexpr auto name() { return "VBox"; }
        const char* l1 = "label 5";
        const char* l2 = "label 6";
      } a_vbox;
    } a_tabs;

    struct
    {
      static constexpr auto layout()
      {
        enum
        {
          split
        } d{};
        return d;
      }
      static constexpr auto name() { return "split"; }
      static constexpr auto width() { return 400; }
      static constexpr auto height() { return 200; }
      const char* l1 = "some long foo";
      const char* l2 = "other bar";
    } a_split;

    struct
    {
      static constexpr auto name() { return "Grid"; }
      static constexpr auto layout()
      {
        enum
        {
          grid
        } d{};
        return d;
      }
      static constexpr auto background()
      {
        enum
        {
          lighter
        } d{};
        return d;
      }
      static constexpr auto columns() { return 3; }
      static constexpr auto padding() { return 5; }

      struct
      {
        static constexpr auto layout()
        {
          enum
          {
            control
          } d{};
          return d;
        }
        static constexpr auto scale() { return 0.8; }

        decltype(&ins::float_ctl) control = &ins::float_ctl;
      } float_widget;

      const char* l1 = "A";
      const char* l2 = "B";
      const char* l3 = "C";
      const char* l4 = "D";
      const char* l5 = "E";
    } a_grid;
  };
  /*
  struct layout {
      enum { vbox };
      static constexpr auto name() { return "Main"; }
      static constexpr auto background() { enum { mid } d{}; return d; }
      struct {
        enum { hbox };
        static constexpr auto background() { enum { dark } d{}; return d; }
        decltype(&ins::int_ctl) int_widget = &ins::int_ctl;
      } widgets;

      struct {
          enum { spacing };
          static constexpr auto width() { return 20; }
          static constexpr auto height() { return 20; }
      } a_spacing;

      struct {
        enum { group };
        static constexpr auto background() { enum { light } d{}; return d; }
        static constexpr auto name() { return "Group"; }
        struct {
          enum { hbox };
          const char* l1 = "label 1";
          struct {
              enum { spacing };
              static constexpr auto width() { return 20; }
              static constexpr auto height() { return 10; }
          } b_spacing;
          const char* l2 = "label 2";
        } a_hbox;
      } b_group;


      struct {
        enum { tabs };
        static constexpr auto name() { return "Tabs"; }
        static constexpr auto background() { enum { darker } d{}; return d; }

        struct {
          enum { hbox };
          static constexpr auto name() { return "HBox"; }
          const char* l1 = "label 3";
          const char* l2 = "label 4";
        } a_hbox;

        struct {
          enum { vbox };
          static constexpr auto name() { return "VBox"; }
          const char* l1 = "label 5";
          const char* l2 = "label 6";
        } a_vbox;
      } a_tabs;

      struct {
        enum { split };
        static constexpr auto name() { return "split"; }
        static constexpr auto width() { return 400; }
        static constexpr auto height() { return 200; }
        const char* l1 = "some long foo";
        const char* l2 = "other bar";
      } a_split;

      struct {
        enum { grid };
        static constexpr auto name() { return "Grid"; }
        static constexpr auto background() { enum { lighter } d{}; return d; }
        static constexpr auto columns() { return 3; }
        static constexpr auto padding() { return 5; }

        struct {
          enum { control };
          decltype(&ins::float_ctl) widget = &ins::float_ctl;
          static constexpr auto scale() { return 0.8; }
        } float_widget;
        const char* l1 = "A";
        const char* l2 = "B";
        const char* l3 = "C";
        const char* l4 = "D";
        const char* l5 = "E";
      } a_grid;
  };*/
};
}
