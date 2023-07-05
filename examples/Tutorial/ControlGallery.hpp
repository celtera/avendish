#pragma once
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/common/for_nth.hpp>
#include <boost/pfr.hpp>
#include <cmath>
#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/controls_fmt.hpp>
#include <ossia/network/value/format_value.hpp>

#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>

#if __has_include(<magic_enum.hpp>)
#include <magic_enum.hpp>
#endif
namespace examples
{

struct ControlGallery
{
  halp_meta(name, "Control gallery");
  halp_meta(c_name, "control_gallery");
  halp_meta(category, "Demo");
  halp_meta(author, "<AUTHOR>");
  halp_meta(description, "<DESCRIPTION>");
  halp_meta(uuid, "a9b0e2c6-61e9-45df-a75d-27abf7fb43d7");

  struct
  {
    //! Buttons are level-triggers: true as long as the button is pressed
    halp::accurate<halp::maintained_button<"Press me ! (Button)">> button;

    //! In contrast, impulses are edge-triggers: there is only a value at the moment of the click.
    halp::accurate<halp::impulse_button<"Press me ! (Impulse)">> impulse_button;

    //! Common widgets
    halp::accurate<halp::hslider_f32<"Float slider", halp::range{0., 1., 0.5}>>
        float_slider;
    halp::accurate<halp::knob_f32<"Float knob", halp::range{0., 1., 0.5}>> float_knob;
    //// // FIXME
    //// struct {
    ////   // FIXME meta_control(Control::LogFloatSlider, "Float slider (log)", 0., 1., 0.5);
    ////   ossia::timed_vec<float> values{};
    //// } log_float_slider;
    ////

#if defined(__clang__) || defined(_MSC_VER)
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=104720
    halp::accurate<halp::hslider_i32<"Int slider", halp::range{0., 1000., 10.}>>
        int_slider;
    halp::accurate<halp::spinbox_i32<"Int spinbox", halp::range{0, 1000, 10}>>
        int_spinbox;
#endif

    //! Will look like a checkbox
    halp::accurate<halp::toggle<"Toggle", halp::toggle_setup{.init = true}>> toggle;

    //! Same, but allows to choose what is displayed.
    // FIXME halp::accurate<halp::chooser_toggle<"Toggle", {"Falsey", "Truey"}, false>> chooser_toggle;

    //! Allows to edit some text.
    halp::accurate<halp::lineedit<"Line edit", "Henlo">> lineedit;

    //! First member of the pair is the text, second is the value.
    //! Defining comboboxes and enumerations is a tiny bit more complicated
    struct : halp::sample_accurate_values<halp::combo_pair<float>>
    {
      halp_meta(name, "Combo box");
      enum widget
      {
        combobox
      };

      struct range
      {
        halp::combo_pair<float> values[3]{{"Foo", -10.f}, {"Bar", 5.f}, {"Baz", 10.f}};
        int init{1}; // Bar
      };

      float value{};
    } combobox;

    //! Here value will be the string
    struct : halp::sample_accurate_values<std::string_view>
    {
      halp_meta(name, "Enum 2");
      enum widget
      {
        enumeration
      };

      struct range
      {
        std::string_view values[4]{"Roses", "Red", "Violets", "Blue"};
        int init{1}; // Red
      };

      // FIXME: string_view: allow outside bounds
      std::string_view value;
    } enumeration_a;

    //! Here value will be the index of the string... but even better than that
    //! is below:
    struct : halp::sample_accurate_values<int>
    {
      halp_meta(name, "Enum 3");
      enum widget
      {
        enumeration
      };

      struct range
      {
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
    struct enum_t
    {
      halp__enum("Simple Enum", Peg, Square, Peg, Round, Hole)
    };
    halp::accurate<enum_t> simpler_enumeration;

    struct combobox_t
    {
      halp__enum_combobox("Color", Blue, Red, Green, Teal, Blue, Black, Orange)
    };
    halp::accurate<combobox_t> simpler_enumeration_in_a_combobox;

    //! Crosshair XY chooser
    halp::accurate<halp::xy_pad_f32<"XY", halp::range{-5.f, 5.f, 0.f}>> position;

    //! Color chooser. Colors are in 8-bit RGBA by default.
    halp::accurate<halp::color_chooser<"Color">> color;

  } inputs;

  void operator()()
  {
    const bool has_impulse = !inputs.impulse_button.values.empty();
    const bool has_button = std::any_of(
        inputs.button.values.begin(), inputs.button.values.end(),
        [](const auto& p) { return p.second == true; });

    if(!has_impulse && !has_button)
      return;

    avnd::for_each_field_ref(inputs, []<typename Control>(const Control& input) {
      {
        auto val = input.values.begin()->second;
        if constexpr(std::is_enum_v<decltype(val)>) {
#if __has_include(<magic_enum.hpp>)
          fmt::print("changed: {} {}", Control::name(), magic_enum::enum_name(val));
#else
          fmt::print("changed: {} {}", Control::name(), std::to_underlying(val));
#endif
        } else {
          fmt::print("changed: {} {}", Control::name(), val);
        }
      }
    });
  }
};

}
