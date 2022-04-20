#include <avnd/wrappers/avnd.hpp>
#include <avnd/concepts/object.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>

#include <examples/Helpers/Controls.hpp>
#include <examples/Raw/Lowpass.hpp>
#include <examples/Raw/Callback.hpp>
#include <examples/Raw/Messages.hpp>
#include <examples/Raw/Init.hpp>
#include <examples/Ports/Essentia/stats/Entropy.hpp>
#include <examples/Ports/VB/vb.fourses_tilde.hpp>

#if __has_include(<fmt/format.h>)
#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/ranges.h>

#include <boost/core/demangle.hpp>

template<typename T>
void print_metadatas()
{
  fmt::print("Effect informations:\n"
             " - Name: '{}'\n"
             " - Name (C): '{}'\n"
             " - UUID: '{}'\n"
             " - Vendor: '{}'\n"
             " - Author: '{}'\n"
             " - Version: '{}'\n"
             , avnd::get_name<T>()
             , avnd::get_c_name<T>()
             , avnd::get_uuid<T>()
             , avnd::get_vendor<T>()
             , avnd::get_author<T>()
             , avnd::get_version<T>()
  );

  fmt::print("\nSupported features:\n"
             " - Initialize: {}\n"
             " - Prepare: {}\n"
             " - Programs: {}\n"
             " - Inputs: {}\n"
             " - Outputs: {}\n"
             " - Messages: {}\n"
             , avnd::can_initialize<T>
             , avnd::can_prepare<T>
             , avnd::has_programs<T>
             , avnd::input_introspection<T>::size
             , avnd::output_introspection<T>::size
             , avnd::messages_introspection<T>::size
             );

  fmt::print("\nAudio channels (-1 means dynamic):\n"
             " - Input channels: {}\n"
             " - Output channels: {}\n"
             " - Input busses: {}\n"
             " - Output busses: {}\n"
             , avnd::channels_introspection<T>::input_channels
             , avnd::channels_introspection<T>::output_channels
             , avnd::bus_introspection<T>::input_busses
             , avnd::bus_introspection<T>::output_busses
             );
}

template<typename T>
void print_processor_types()
{
  fmt::print("\nSupported processing types:\n");

  if constexpr(avnd::monophonic_audio_processor<T> || avnd::polyphonic_audio_processor<T>)
  {
    if constexpr(avnd::mono_per_sample_arg_processor<float, T>)
      fmt::print(" - Monophonic processor (per-sample, float, as arguments)\n");
    if constexpr(avnd::mono_per_sample_arg_processor<double, T>)
      fmt::print(" - Monophonic processor (per-sample, double, as arguments)\n");
    if constexpr(avnd::mono_per_sample_port_processor<float, T>)
      fmt::print(" - Monophonic processor (per-sample, float, as ports)\n");
    if constexpr(avnd::mono_per_sample_port_processor<double, T>)
      fmt::print(" - Monophonic processor (per-sample, double, as ports)\n");

    if constexpr(avnd::monophonic_arg_audio_effect<float, T>)
       fmt::print(" - Monophonic processor (mono array, float, as arguments)\n");
    if constexpr(avnd::mono_per_sample_port_processor<double, T>)
        fmt::print(" - Monophonic processor (mono array, double, as arguments)\n");

    if constexpr(avnd::poly_array_port_based<float, T>)
      fmt::print(" - Polyphonic processor (poly array, float, as ports)\n");
    if constexpr(avnd::poly_array_port_based<double, T>)
      fmt::print(" - Polyphonic processor (poly array, double, as ports)\n");
  }
  else
  {
    fmt::print(" - Message processor\n");
  }

  fmt::print("\n");
}

template<typename Field>
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

template<typename T>
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

template<typename Field>
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


template<typename Processor>
void print_io()
{
  if constexpr(avnd::input_introspection<Processor>::size > 0)
  {
    fmt::print("Inputs:\n");
    int k = 0;

    avnd::input_introspection<Processor>::for_all(
          [&] <typename Field> (Field wrap) {
            using type = typename Field::type;
            fmt::print(" - Port index: {}\n", k++);
            fmt::print("   - Type: {}\n", port_type(wrap));
            fmt::print("   - Name: '{}'\n", avnd::get_name<type>());

            fmt::print("\n");
          });
  }

  if constexpr(avnd::output_introspection<Processor>::size > 0)
  {
    fmt::print("Outputs:\n");
    int k = 0;
    avnd::output_introspection<Processor>::for_all(
          [&] <typename Field> (Field wrap) {
            using type = typename Field::type;
            fmt::print(" - Port index: {}\n", k++);
            fmt::print("   - Type: {}\n", port_type(wrap));
            fmt::print("   - Name: '{}'\n", avnd::get_name<type>());

            fmt::print("\n");
          });
  }
}

template<typename Processor>
void print_parameters()
{
  if constexpr(avnd::parameter_input_introspection<Processor>::size > 0)
  {
    fmt::print("Parameters:\n");
    int k = 0;
    avnd::parameter_input_introspection<Processor>::for_all(
          [&] <typename Field> (Field wrap) {
            using type = typename Field::type;
            fmt::print(" - Parameter index: {}\n", k++);
            fmt::print("   - Name: '{}'\n", avnd::get_name<type>());
            fmt::print("   - Control type: {}\n", parameter_value_type(wrap));

            if constexpr(avnd::parameter_with_minmax_range<type>) {
              constexpr auto ctl = avnd::get_range<type>();
              if constexpr(requires { ctl.min; })
                fmt::print("   - Min: {}\n", ctl.min);
              if constexpr(requires { ctl.max; })
                fmt::print("   - Max: {}\n", ctl.max);
              if constexpr(requires { ctl.init; })
                fmt::print("   - Init: {}\n", ctl.init);
            }
            else if constexpr(avnd::enum_parameter<type>)
            {
              fmt::print("   - Choices: {}\n", avnd::get_enum_choices<type>());
            }
            else
            {
              fmt::print("   - Default value: {}\n", type{}.value);
            }

            fmt::print("\n");
          });
  }
}

template<typename refl>
void print_message(refl)
{
  using args = typename refl::arguments;
  fmt::print("   - Arguments: ");
  if constexpr(refl::count == 0) {
    fmt::print("<none>");
  }
  else
  {
    boost::mp11::mp_for_each<boost::mp11::mp_iota_c<refl::count>>([] (auto I) {
      fmt::print("{}, ", type_name<boost::mp11::mp_at_c<args, I>>());
    });
  }
  fmt::print("\n");

  using ret = typename refl::return_type;
  fmt::print("   - Return type: {}\n", type_name<ret>());
}

template<typename Processor>
void print_messages()
{
  if constexpr(avnd::messages_introspection<Processor>::size > 0)
  {
    fmt::print("Message:\n");
    int k = 0;
    avnd::messages_introspection<Processor>::for_all(
          [&] <typename Field> (Field wrap) {
            using type = typename Field::type;

            fmt::print(" - Message index: {}\n", k++);
            fmt::print("   - Name: '{}'\n", avnd::get_name<type>());

            if constexpr(requires (Processor p, std::vector<std::variant<float>> v) { type::func()(p, v); })
            {
              fmt::print("   - Type: generic range function\n");
            }
            else
            {
              if constexpr(avnd::stateless_message<type>)
              {
                if constexpr (std::is_member_function_pointer_v<decltype(type::func())>)
                  fmt::print("   - Type: member-function-pointer\n");
                else
                  fmt::print("   - Type: standalone function\n");
              }
              else if constexpr (avnd::stateful_message<type>)
              {
                fmt::print("   - Type: func() returns a function object\n");
              }
              else if constexpr (avnd::stdfunc_message<type>)
              {
                fmt::print("   - Type: std::function\n");
              }
              else if constexpr (avnd::function_object_message<type>)
              {
                fmt::print("   - Type: the message is a function object\n");
              }

              print_message(avnd::message_function_reflection<type>());
            }

            fmt::print("\n");
          }
    );
  }
}

template<typename Processor>
void print_callbacks()
{
  if constexpr(avnd::callback_output_introspection<Processor>::size > 0)
  {
    fmt::print("Callback:\n");
    int k = 0;
    avnd::callback_output_introspection<Processor>::for_all(
          [&] <typename Field> (Field wrap) {
            using type = typename Field::type;

            fmt::print(" - Callback index: {}\n", k++);
            fmt::print("   - Name: '{}'\n", avnd::get_name<type>());

            if constexpr(requires { avnd::function_reflection_t<decltype(type::call)>{}; })
            {
              print_callback(avnd::function_reflection_t<decltype(type::call)>{});
            }

            fmt::print("\n");
          }
          );
  }
}

template<typename Processor>
void dump()
{
  print_metadatas<Processor>();

  print_processor_types<Processor>();

  print_io<Processor>();

  print_messages<Processor>();

  print_parameters<Processor>();

  print_callbacks<Processor>();
}

int main()
{
  dump<vb_ports::fourses_tilde<halp::basic_logger>>();
}
#else
int main()
{

}
#endif
