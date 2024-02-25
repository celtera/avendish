#define BOOST_NO_RTTI 1
#include <avnd/concepts/all.hpp>
#include <avnd/concepts/object.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <examples/Helpers/Controls.hpp>
#include <examples/Helpers/Messages.hpp>
#include <examples/Ports/Essentia/stats/Entropy.hpp>
#include <examples/Raw/Callback.hpp>
#include <examples/Raw/Init.hpp>
#include <examples/Raw/Lowpass.hpp>
#include <examples/Raw/Messages.hpp>

#include <examples/Ports/VB/vb.fourses_tilde.hpp>

#if __has_include(<fmt/format.h>)
#include <boost/core/demangle.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>

FILE* outfile{};
template <typename T>
void print_metadatas()
{
  fmt::print(
      outfile,
      "Effect informations:\n"
      " - Name: '{}'\n"
      " - Name (C): '{}'\n"
      " - UUID: '{}'\n"
      " - Vendor: '{}'\n"
      " - Author: '{}'\n"
      " - Version: '{}'\n",
      avnd::get_name<T>(), avnd::get_c_name<T>(), avnd::get_uuid<T>(),
      avnd::get_vendor<T>(), avnd::get_author<T>(), avnd::get_version<T>());

  fmt::print(
      outfile,
      "\nSupported features:\n"
      " - Initialize: {}\n"
      " - Prepare: {}\n"
      " - Programs: {}\n"
      " - Inputs: {}\n"
      " - Outputs: {}\n"
      " - Messages: {}\n",
      avnd::can_initialize<T>, avnd::can_prepare<T>, avnd::has_programs<T>,
      avnd::input_introspection<T>::size, avnd::output_introspection<T>::size,
      avnd::messages_introspection<T>::size);

  fmt::print(
      outfile,
      "\nAudio channels (-1 means dynamic):\n"
      " - Input channels: {}\n"
      " - Output channels: {}\n"
      " - Input busses: {}\n"
      " - Output busses: {}\n",
      avnd::channels_introspection<T>::input_channels,
      avnd::channels_introspection<T>::output_channels,
      avnd::bus_introspection<T>::input_busses,
      avnd::bus_introspection<T>::output_busses);
}

template <typename T>
void print_processor_types()
{
  fmt::print(outfile, "\nSupported processing types:\n");

  if constexpr(
      avnd::monophonic_audio_processor<T> || avnd::polyphonic_audio_processor<T>)
  {
    if constexpr(avnd::mono_per_sample_arg_processor<float, T>)
      fmt::print(outfile, " - Monophonic processor (per-sample, float, as arguments)\n");
    if constexpr(avnd::mono_per_sample_arg_processor<double, T>)
      fmt::print(
          outfile, " - Monophonic processor (per-sample, double, as arguments)\n");
    if constexpr(avnd::mono_per_sample_port_processor<float, T>)
      fmt::print(outfile, " - Monophonic processor (per-sample, float, as ports)\n");
    if constexpr(avnd::mono_per_sample_port_processor<double, T>)
      fmt::print(outfile, " - Monophonic processor (per-sample, double, as ports)\n");

    if constexpr(avnd::monophonic_arg_audio_effect<float, T>)
      fmt::print(outfile, " - Monophonic processor (mono array, float, as arguments)\n");
    if constexpr(avnd::mono_per_sample_port_processor<double, T>)
      fmt::print(
          outfile, " - Monophonic processor (mono array, double, as arguments)\n");

    if constexpr(avnd::poly_array_port_based<float, T>)
      fmt::print(outfile, " - Polyphonic processor (poly array, float, as ports)\n");
    if constexpr(avnd::poly_array_port_based<double, T>)
      fmt::print(outfile, " - Polyphonic processor (poly array, double, as ports)\n");
  }
  else
  {
    fmt::print(outfile, " - Message processor\n");
  }

  fmt::print(outfile, "\n");
}

template <typename Field>
std::string_view port_type(Field f)
{
  using type = typename Field::type;
  if constexpr(avnd::audio_sample_port<float, type>)
    return "Audio port (sample, float)";
  else if constexpr(avnd::audio_sample_port<double, type>)
    return "Audio port (sample, double)";
  else if constexpr(avnd::mono_array_sample_port<float, type>)
    return "Audio port (mono channel, float)";
  else if constexpr(avnd::mono_array_sample_port<double, type>)
    return "Audio port (mono channel, double)";
  else if constexpr(avnd::poly_array_sample_port<float, type>)
    return "Audio port (bus, float)";
  else if constexpr(avnd::poly_array_sample_port<double, type>)
    return "Audio port (bus, double)";
  else if constexpr(avnd::midi_port<type>)
    return "MIDI port";
  else if constexpr(avnd::parameter<type>)
    return "Parameter";
  else if constexpr(avnd::message<type>)
    return "Message";
  else if constexpr(avnd::callback<type>)
    return "Callback";
  else
    return "<unknown>";
}

template <typename T>
auto type_name()
{
  if constexpr(std::is_same_v<T, int>)
    return "int";
  else if constexpr(std::is_same_v<T, float>)
    return "float";
  else if constexpr(std::is_same_v<T, double>)
    return "double";
  else if constexpr(std::is_same_v<T, const char*>)
    return "const char*";
  else if constexpr(std::is_same_v<T, bool>)
    return "bool";
  else if constexpr(std::is_same_v<T, void>)
    return "void";
  else if constexpr(std::is_same_v<T, std::string>)
    return "std::string";
  else if constexpr(std::is_same_v<T, std::string_view>)
    return "std::string_view";
  else
  {
    return boost::core::demangle(typeid(T).name());
  }
}

template <typename Field>
std::string_view parameter_value_type(Field f)
{
  using type = typename Field::type;
  if constexpr(avnd::int_parameter<type>)
    return "int";
  if constexpr(avnd::float_parameter<type>)
    return "float";
  if constexpr(avnd::bool_parameter<type>)
    return "bool";
  if constexpr(avnd::string_parameter<type>)
    return "string";
  if constexpr(avnd::enum_parameter<type>)
    return "enum";
}

template <typename Processor>
void print_io()
{
  if constexpr(avnd::input_introspection<Processor>::size > 0)
  {
    fmt::print(outfile, "Inputs:\n");
    int k = 0;

    avnd::input_introspection<Processor>::for_all([&]<typename Field>(Field wrap) {
      using type = typename Field::type;
      fmt::print(outfile, " - Port index: {}\n", k++);
      fmt::print(outfile, "   - Type: {}\n", port_type(wrap));
      fmt::print(outfile, "   - Name: '{}'\n", avnd::get_name<type>());

      fmt::print(outfile, "\n");
    });
  }

  if constexpr(avnd::output_introspection<Processor>::size > 0)
  {
    fmt::print(outfile, "Outputs:\n");
    int k = 0;
    avnd::output_introspection<Processor>::for_all([&]<typename Field>(Field wrap) {
      using type = typename Field::type;
      fmt::print(outfile, " - Port index: {}\n", k++);
      fmt::print(outfile, "   - Type: {}\n", port_type(wrap));
      fmt::print(outfile, "   - Name: '{}'\n", avnd::get_name<type>());

      fmt::print(outfile, "\n");
    });
  }
}

template <typename Processor>
void print_parameters()
{
  if constexpr(avnd::parameter_input_introspection<Processor>::size > 0)
  {
    fmt::print(outfile, "Parameters:\n");
    int k = 0;
    avnd::parameter_input_introspection<Processor>::for_all(
        [&]<typename Field>(Field wrap) {
      using type = typename Field::type;
      fmt::print(outfile, " - Parameter index: {}\n", k++);
      fmt::print(outfile, "   - Name: '{}'\n", avnd::get_name<type>());
      fmt::print(outfile, "   - Control type: {}\n", parameter_value_type(wrap));

      if constexpr(avnd::parameter_with_minmax_range<type>)
      {
        constexpr auto ctl = avnd::get_range<type>();
        if constexpr(requires { ctl.min; })
          fmt::print(outfile, "   - Min: {}\n", ctl.min);
        if constexpr(requires { ctl.max; })
          fmt::print(outfile, "   - Max: {}\n", ctl.max);
        if constexpr(requires { ctl.init; })
          fmt::print(outfile, "   - Init: {}\n", ctl.init);
      }
      else if constexpr(avnd::enum_parameter<type>)
      {
        fmt::print(outfile, "   - Choices: {}\n", avnd::get_enum_choices<type>());
      }
      else
      {
        fmt::print(outfile, "   - Default value: {}\n", type{}.value);
      }

      fmt::print(outfile, "\n");
    });
  }
}

template <typename refl>
void print_message(refl)
{
  using args = typename refl::arguments;
  fmt::print(outfile, "   - Arguments: ");
  if constexpr(refl::count == 0)
  {
    fmt::print(outfile, "<none>");
  }
  else
  {
    boost::mp11::mp_for_each<boost::mp11::mp_iota_c<refl::count>>([](auto I) {
      fmt::print(outfile, "{}, ", type_name<boost::mp11::mp_at_c<args, I>>());
    });
  }
  fmt::print(outfile, "\n");

  using ret = typename refl::return_type;
  fmt::print(outfile, "   - Return type: {}\n", type_name<ret>());
}

template <typename Processor>
void print_messages()
{
  if constexpr(avnd::messages_introspection<Processor>::size > 0)
  {
    fmt::print(outfile, "Message:\n");
    int k = 0;
    avnd::messages_introspection<Processor>::for_all([&]<typename Field>(Field wrap) {
      using type = typename Field::type;

      fmt::print(outfile, " - Message index: {}\n", k++);
      fmt::print(outfile, "   - Name: '{}'\n", avnd::get_name<type>());

      if constexpr(requires(Processor p, std::vector<std::variant<float>> v) {
                     type::func()(p, v);
                   })
      {
        fmt::print(outfile, "   - Type: generic range function\n");
      }
      else
      {
        if constexpr(avnd::stateless_message<type>)
        {
          if constexpr(std::is_member_function_pointer_v<decltype(type::func())>)
            fmt::print(outfile, "   - Type: member-function-pointer\n");
          else
            fmt::print(outfile, "   - Type: standalone function\n");
        }
        else if constexpr(avnd::stateful_message<type>)
        {
          fmt::print(outfile, "   - Type: func() returns a function object\n");
        }
        else if constexpr(avnd::stdfunc_message<type>)
        {
          fmt::print(outfile, "   - Type: std::function\n");
        }
        else if constexpr(avnd::function_object_message<type>)
        {
          fmt::print(outfile, "   - Type: the message is a function object\n");
        }

        print_message(avnd::message_function_reflection<type>());
      }

      fmt::print(outfile, "\n");
    });
  }
}

template <typename Processor>
void print_callbacks()
{
  if constexpr(avnd::callback_output_introspection<Processor>::size > 0)
  {
    fmt::print(outfile, "Callback:\n");
    int k = 0;
    avnd::callback_output_introspection<Processor>::for_all(
        [&]<typename Field>(Field wrap) {
      using type = typename Field::type;

      fmt::print(outfile, " - Callback index: {}\n", k++);
      fmt::print(outfile, "   - Name: '{}'\n", avnd::get_name<type>());

      if constexpr(requires { avnd::function_reflection_t<decltype(type::call)>{}; })
      {
        print_callback(avnd::function_reflection_t<decltype(type::call)>{});
      }

      fmt::print(outfile, "\n");
    });
  }
}

void header()
{
  fmt::print(outfile, "#N canvas 733 352 450 300 12;\n");
}

template <typename Processor>
void dump_controls(
    const int cur_x, const int cur_y, const int obj_index, int& num_objects)
{
  // Every control: add a smybol / range slider / ...
  int x = cur_x;
  int y = cur_y;
  avnd::parameter_input_introspection<Processor>::for_all(
      [&]<typename Field>(Field wrap) {
    using type = typename Field::type;
    constexpr std::string_view name = avnd::get_name<type>();
    y = cur_y;

    std::string default_arg;
    std::optional<int> control_index;
    if constexpr(avnd::int_parameter<type>)
    {
      fmt::print(outfile, "#X floatatom {} {} 5 0 0 0 - - - 0;\n", x, y);
      control_index = num_objects;
      num_objects++;
      y += 20;
    }
    else if constexpr(avnd::enum_parameter<type>)
    {
    }
    else if constexpr(avnd::float_parameter<type>)
    {
      auto range = avnd::get_range<type>();

      fmt::print(
          outfile,
          "#X obj {} {} hsl 128 15 {} {} 0 0 empty empty empty -2 -8 0 10 #fcfcfc "
          "#000000 #000000 0 1;\n",
          x, y, range.min, range.max);
      control_index = num_objects;
      num_objects++;
      y += 20;
    }
    else if constexpr(avnd::bool_parameter<type>)
    {
      fmt::print(
          outfile,
          "#X obj {} {} tgl 15 0 empty empty empty 17 7 0 10 #fcfcfc #000000 #000000 0 "
          "1;\n",
          x, y);

      control_index = num_objects;
      num_objects++;
      y += 20;
    }
    else if constexpr(avnd::string_parameter<type>)
    {
    }

    // Add a message object
    fmt::print(outfile, "#X msg {} {} {} \\$1;\n", x, y, name);
    int msg_index = num_objects;
    int msg_out_port = 0;
    int obj_in_port = 0;
    x += 40;
    num_objects++;

    // Connect control to message
    if(control_index)
    {
      int control_out_port = 0;
      int msg_in_port = 0;
      fmt::print(
          outfile, "#X connect {} {} {} {};\n", *control_index, control_out_port,
          msg_index, msg_in_port);
    }

    // Connect message to object
    fmt::print(
        outfile, "#X connect {} {} {} {};\n", msg_index, msg_out_port, obj_index,
        obj_in_port);
  });
  // Enum: add the symbols
}

template <typename Processor>
void dump_messages(
    const int cur_x, const int cur_y, const int obj_index, int& num_objects)
{
  int x = cur_x;
  int y = cur_y;
  avnd::messages_introspection<Processor>::for_all([&]<typename Field>(Field wrap) {
    using type = typename Field::type;
    constexpr auto name = avnd::get_name<type>();

    std::string default_arg;
    if constexpr(avnd::int_parameter<type>)
    {
      default_arg = "1";
    }
    else if constexpr(avnd::enum_parameter<type>)
    {
      default_arg = "";
    }
    else if constexpr(avnd::float_parameter<type>)
    {
      default_arg = "0.5";
    }
    else if constexpr(avnd::bool_parameter<type>)
    {
      default_arg = "true";
    }
    else if constexpr(avnd::string_parameter<type>)
    {
      default_arg = "hello";
    }

    // Add a message object
    fmt::print(outfile, "#X msg {} {} {} {};\n", x, y, name, default_arg);
    int msg_index = num_objects;
    int msg_out_port = 0;
    int obj_in_port = 0;
    x += 40;
    num_objects++;

    // Connect message to object
    fmt::print(
        outfile, "#X connect {} {} {} {};\n", msg_index, msg_out_port, obj_index,
        obj_in_port);
  });
}

template <typename Processor>
void dump_audio()
{
  header();

  // Every audio input:
  // add an osc~ objet.
  static constexpr const int input_channels = avnd::input_channels<Processor>(1);
  static constexpr const int output_channels = avnd::output_channels<Processor>(1);

  int num_objects = 0;
  // Every audio output: *~ and a gain to prevent pops

  int x = 30;
  int y = 30;
  for(int i = 0; i < input_channels; i++)
  {
    // Add input
    fmt::print(outfile, "#X obj {} {} osc~ 220;\n", x, y);
    x += 30;
    num_objects++;
  }

  // Add the object
  x = 30;
  y = 30 + 50 * 2;

  fmt::print(outfile, "#X obj {} {} {};\n", x, y, avnd::get_c_name<Processor>());
  num_objects++;

  y = 30 + 50 * 3;
  for(int i = 0; i < output_channels; i++)
  {
    // Add output
    fmt::print(outfile, "#X obj {} {} clip~ -0.95 0.95;\n", x, y);
    num_objects++;
    x += 30;
  }

  // Add dac~
  x = 30;
  y = 30 + 50 * 4;
  fmt::print(outfile, "#X obj {} {} dac~;\n", x, y);
  num_objects++;

  // Connect
  // Each input to the object
  {
    for(int i = 0; i < input_channels; i++)
    {
      const int osc_index = i;
      const int osc_out_port = 0;

      const int obj_index = input_channels;
      const int obj_in_port = i;

      fmt::print(
          outfile, "#X connect {} {} {} {};\n", osc_index, osc_out_port, obj_index,
          obj_in_port);
    }
  }

  // Each object input to the clip and each clip to the dac
  {
    for(int i = 0; i < output_channels; i++)
    {
      const int obj_index = input_channels;
      const int obj_out_port = i;

      const int clip_index = obj_index + i + 1;
      const int clip_in_port = 0;
      const int clip_out_port = 0;

      const int dac_index = obj_index + output_channels + 1;
      const int dac_in_port = i;
      fmt::print(
          outfile, "#X connect {} {} {} {};\n", obj_index, obj_out_port, clip_index,
          clip_in_port);
      fmt::print(
          outfile, "#X connect {} {} {} {};\n", clip_index, clip_out_port, dac_index,
          dac_in_port);
    }
  }

  x = 50;
  y = 30 + 50;
  dump_controls<Processor>(x, y, input_channels, num_objects);
  // Every message: ...
  x = 70;
  y = 30 + 80;
  dump_messages<Processor>(x, y, input_channels, num_objects);

  // Every callback: ...
}

template <typename Processor>
void dump_message()
{
  header();
  int num_objects = 0;

  // Add the object
  int x = 30;
  int y = 30 + 50 * 2;

  fmt::print(outfile, "#X obj {} {} {};\n", x, y, avnd::get_c_name<Processor>());
  num_objects++;

  // Add a print
  x = 30;
  y = 30 + 50 * 4;
  fmt::print(outfile, "#X obj {} {} print;\n", x, y);
  num_objects++;

  x = 50;
  y = 30 + 50;
  dump_controls<Processor>(x, y, 1, num_objects);

  x = 70;
  y = 30 + 80;
  dump_messages<Processor>(x, y, 1, num_objects);
}

#include <fstream>
#include <iostream>
int main(int argc, char** argv)
{
  if(argc < 2)
  {
    fprintf(stderr, "Usage: generate_pd_help path/to/out_file.pd");
    return 1;
  }
  auto path = argv[1];

  {
    outfile = fopen(path, "w");

    //using type = examples::Lowpass;
    using type = examples::helpers::Messages<halp::basic_logger>;
    //  using type = vb_ports::fourses_tilde<halp::basic_logger>;
    if constexpr(
        avnd::monophonic_audio_processor<type> || avnd::polyphonic_audio_processor<type>)

      dump_audio<type>();
    else
      dump_message<type>();

    fclose(outfile);
  }
}
#else
int main() { }
#endif
