#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/helpers/audio.hpp>
#include <avnd/helpers/controls.hpp>
#include <avnd/helpers/meta.hpp>
#include <avnd/helpers/sample_accurate_controls.hpp>
/*
#include <ossia/detail/logger.hpp>
#include <ossia/network/value/format_value.hpp>
*/
#include <fmt/format.h>
#include <avnd/helpers/sample_accurate_controls.hpp>
#include <boost/pfr.hpp>

#include <cmath>


template<typename T>
struct fmt::formatter<avnd::combo_pair<T>>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
      return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const avnd::combo_pair<T>& number, FormatContext& ctx)
    {
      return fmt::format_to(ctx.out(), "combo: {}->{}", number.first, number.second);
    }
};

template<typename T>
struct fmt::formatter<avnd::xy_type<T>>
{
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
      return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const avnd::xy_type<T>& number, FormatContext& ctx)
    {
      return fmt::format_to(ctx.out(), "xy: {}, {}", number.x, number.y);
    }
};
template<>
struct fmt::formatter<avnd::color_type>
{
  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const avnd::color_type& number, FormatContext& ctx)
  {
    return fmt::format_to(ctx.out(), "rgba: {}, {}, {}, {}", number.r, number.g, number.b, number.a);
  }
};

namespace examples
{

struct ControlGallery
{
  $(name, "Control gallery");
  $(script_name, "control_gallery");
  $(category, "Demo");
  $(author, "<AUTHOR>");
  $(description, "<DESCRIPTION>");
  $(uuid, "a9b0e2c6-61e9-45df-a75d-27abf7fb43d7");

  struct {
    //! Buttons are level-triggers: true as long as the button is pressed
    avnd::accurate<avnd::maintained_button<"Press me ! (Button)">> button;

    //! In contrast, impulses are edge-triggers: there is only a value at the moment of the click.
    avnd::accurate<avnd::impulse_button<"Press me ! (Impulse)">> impulse_button;

    //! Common widgets
    avnd::accurate<avnd::hslider_f32<"Float slider", avnd::range{0., 1., 0.5}>> float_slider;
    avnd::accurate<avnd::knob_f32<"Float knob", avnd::range{0., 1., 0.5}>> float_knob;
    //// // FIXME
    //// struct {
    ////   // FIXME meta_control(Control::LogFloatSlider, "Float slider (log)", 0., 1., 0.5);
    ////   ossia::timed_vec<float> values{};
    //// } log_float_slider;
    ////

#if defined(__clang__) || defined(_MSC_VER)
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=104720
    avnd::accurate<avnd::hslider_i32<"Int slider", avnd::range{0., 1000., 10.}>> int_slider;
    avnd::accurate<avnd::spinbox_i32<"Int spinbox", avnd::range{0, 1000, 10}>> int_spinbox;
#endif

    //! Will look like a checkbox
    avnd::accurate<avnd::toggle<"Toggle", avnd::toggle_setup{.init = true}>> toggle;

    //! Same, but allows to choose what is displayed.
    // FIXME avnd::accurate<avnd::chooser_toggle<"Toggle", {"Falsey", "Truey"}, false>> chooser_toggle;

    //! Allows to edit some text.
    avnd::accurate<avnd::lineedit<"Line edit", "Henlo">> lineedit;

    //! First member of the pair is the text, second is the value.
    //! Defining comboboxes and enumerations is a tiny bit more complicated
    struct : avnd::sample_accurate_values<avnd::combo_pair<float>> {
      $(name, "Combo box");
      enum widget { combobox };

      struct range {
        avnd::combo_pair<float> values[3]{{"Foo", -10.f}, {"Bar", 0.f}, {"Baz", 10.f}};
        int init{1}; // Bar
      };

      avnd::combo_pair<float> value;
    } combobox;

    //! Here value will be the string
    struct : avnd::sample_accurate_values<std::string_view> {
        $(name, "Enum 2");
        enum widget { enumeration };

        struct range {
          std::string_view values[4]{"Roses", "Red", "Violets", "Blue"};
          int init{1}; // Red
        };

        // FIXME: string_view: allow outside bounds
        std::string_view value;
    } enumeration_a;

    //! Here value will be the index of the string... but even better than that
    //! is below:
    struct : avnd::sample_accurate_values<int> {
        $(name, "Enum 3");
        enum widget { enumeration };

        struct range {
          std::string_view values[4]{"Roses 2", "Red 2", "Violets 2", "Blue 2"};
          int init{1}; // Red
        };

        int value{};
    } enumeration_b;


    /// // FIXME
    /// //! Same as Enum but won't reject strings that are not part of the list.
    /// struct {
    ///   static const constexpr std::array<const char*, 3> choices() {
    ///     return {"Square", "Sine", "Triangle"};
    ///   };
    ///   // FIXME meta_control(Control::UnvalidatedEnum, "Unchecked enum", 1, choices());
    ///   ossia::timed_vec<std::string> values{};
    /// } unvalidated_enumeration;

    //! It's also possible to use this which will define an enum type and
    //! map to it automatically.
    //! e.g. in source one can then do:
    //!
    //!   auto& param = inputs.simpler_enumeration;
    //!   using enum_type = decltype(param)::enum_type;
    //!   switch(param.value) {
    //!      case enum_type::Square:
    //!        ...
    //!   }
    //!
    //! OSC messages can use either the int index or the string.
    using enum_t = avnd__enum("Simple Enum", Peg, Square, Peg, Round, Hole);
    avnd::accurate<enum_t> simpler_enumeration;

    //! Crosshair XY chooser
    avnd::accurate<avnd::xy_pad_f32<"XY", avnd::range{-5.f, 5.f, 0.f}>> position;

    //! Color chooser. Colors are in 8-bit RGBA by default.
    avnd::accurate<avnd::color_chooser<"Color">> color;

  } inputs;

  void operator()()
  {
    const bool has_impulse = !inputs.impulse_button.values.empty();
    const bool has_button = std::any_of(
                inputs.button.values.begin(),
                inputs.button.values.end(),
                [] (const auto& p) { return p.second == true; });

    if(!has_impulse && !has_button)
      return;
/*
    ossia::logger().debug("");
    boost::pfr::for_each_field(
        inputs,
        [] <typename Control> (const Control& input) {
          {
            auto val = input.values.begin()->second;
            if constexpr(!std::is_same_v<decltype(val), ossia::impulse>)
              ossia::logger().critical("changed: {} {}", Control::name(), val);
          }
    });*/
  }
};

}
