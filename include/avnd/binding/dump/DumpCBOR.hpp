#pragma once

#include <avnd/concepts/all.hpp>
#include <avnd/introspection/channels.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/avnd.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <avnd/binding/dump/json_writer.hpp>
#include <boost/core/demangle.hpp>

#include <fstream>
#include <iostream>
#include <tuple>
#include <variant>
#include <vector>

namespace dump_cbor
{
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

template <typename T>
void print_metadatas(dump_json::value j)
{
  using all_properties = std::tuple<
      avnd::prop_name, avnd::prop_c_name, avnd::prop_uuid, avnd::prop_vendor,
      avnd::prop_product, avnd::prop_version, avnd::prop_label, avnd::prop_short_label,
      avnd::prop_category, avnd::prop_copyright, avnd::prop_license, avnd::prop_url,
      avnd::prop_email, avnd::prop_manual_url, avnd::prop_support_url,
      avnd::prop_description>;

  auto test = [&j]<typename Arg>(Arg& args) {
    if constexpr(Arg::template has<T>())
      j[args.name()] = args.template get<T>();
  };
  std::apply([&](auto&&... args) { (test(args), ...); }, all_properties{});
}

template <typename T>
void print_supported_features(dump_json::value j)
{
#define check_has_feature(feature)       \
  do                                     \
    if constexpr(avnd::has_##feature<T>) \
    {                                    \
      j[#feature] = true;                \
    }                                    \
  while(0)

#define check_has_feature_qualified(feature)        \
  do                                                \
    if constexpr(avnd::has_##feature<T>)            \
    {                                               \
      if constexpr(avnd::feature##_is_value<T>)     \
        j[#feature] = "value";                      \
      else if constexpr(avnd::feature##_is_type<T>) \
        j[#feature] = "type";                       \
      else                                          \
        j[#feature] = "unknown";                    \
    }                                               \
  while(0)

#define check_can_feature(feature)       \
  do                                     \
    if constexpr(avnd::can_##feature<T>) \
    {                                    \
      j[#feature] = true;                \
    }                                    \
  while(0)

#define check_tag_feature(feature)       \
  do                                     \
    if constexpr(avnd::tag_##feature<T>) \
    {                                    \
      j[#feature] = true;                \
    }                                    \
  while(0)

  check_has_feature(programs);
  check_can_feature(bypass);
  check_can_feature(prepare);
  check_tag_feature(skip_init);
  check_can_feature(initialize);
  check_has_feature(clock);

  check_has_feature_qualified(schedule);
  check_has_feature_qualified(clocks);
  check_has_feature_qualified(ui);
  check_has_feature_qualified(messages);
  check_has_feature_qualified(inputs);
  check_has_feature_qualified(outputs);

  /*
  fmt::print(
      "\nAudio channels (-1 means dynamic):\n"
      " - Input channels: {}\n"
      " - Output channels: {}\n"
      " - Input busses: {}\n"
      " - Output busses: {}\n",
      avnd::channels_introspection<T>::input_channels,
      avnd::channels_introspection<T>::output_channels,
      avnd::bus_introspection<T>::input_busses,
      avnd::bus_introspection<T>::output_busses);
*/
}

template <typename T>
void print_processor_types()
{
  /*
  fmt::print("\nSupported processing types:\n");

  if constexpr(
      avnd::monophonic_audio_processor<T> || avnd::polyphonic_audio_processor<T>)
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
*/
}

template <typename Field>
std::string_view port_type(Field f)
{
  using type = typename Field::type;
  // tensor/buffer/geometry before parameter: a tensor_port also exposes `.value`.
  if constexpr(avnd::audio_port<type>)
    return "audio";
  else if constexpr(avnd::midi_port<type>)
    return "midi";
  else if constexpr(avnd::texture_port<type>)
    return "texture";
  else if constexpr(avnd::buffer_port<type>)
    return "buffer";
  else if constexpr(avnd::tensor_port<type>)
    return "tensor";
  else if constexpr(avnd::geometry_port<type>)
    return "geometry";
  else if constexpr(avnd::sampler_port<type>)
    return "sampler";
  else if constexpr(avnd::attachment_port<type>)
    return "attachment";
  else if constexpr(avnd::image_port<type>)
    return "image";
  else if constexpr(avnd::parameter_port<type>)
    return "parameter";
  else if constexpr(avnd::message<type>)
    return "message";
  else if constexpr(avnd::callback<type>)
    return "callback";
  else
    return "<unknown>";
}

template <typename Field>
std::string_view parameter_value_type(Field f)
{
  using type = typename Field::type;
  if constexpr(avnd::int_parameter<type>)
    return "int";
  else if constexpr(avnd::float_parameter<type>)
    return "float";
  else if constexpr(avnd::bool_parameter<type>)
    return "bool";
  else if constexpr(avnd::string_parameter<type>)
    return "string";
  else if constexpr(avnd::enum_parameter<type>)
    return "enum";
  else if constexpr(avnd::xyzw_parameter<type>)
    return "xyzw";
  else if constexpr(avnd::xyz_parameter<type>)
    return "xyz";
  else if constexpr(avnd::rgba_parameter<type>)
    return "rgba";
  else if constexpr(avnd::rgb_parameter<type>)
    return "rgb";
  else if constexpr(avnd::xy_parameter<type>)
    return "xy";
  else
    return "unknown";
}

template <typename Field>
std::string_view value_type(Field f)
{
  using type = Field;
  if constexpr(avnd::int_ish<type>)
    return "int";
  if constexpr(avnd::fp_ish<type>)
    return "float";
  if constexpr(avnd::bool_ish<type>)
    return "bool";
  if constexpr(avnd::string_ish<type>)
    return "string";
  if constexpr(avnd::enum_ish<type>)
    return "enum";
  return "unknown";
}

template <typename Field>
void print_callback(dump_json::value obj)
{
  using type = typename Field::type;
  if constexpr(avnd::view_callback<type>)
    obj["type"] = "view";
  else if constexpr(avnd::stored_callback<type>)
    obj["type"] = "stored";
  else
    obj["type"] = "unknown";

  using function_type = decltype(type::call);
  using refl = avnd::function_reflection_t<std::decay_t<function_type>>;
  using args = typename refl::arguments;

  auto arr = obj["arguments"];
  arr.ensure_array();
  auto add = [&arr]<typename A>(A&& a) { arr.push_back(value_type(a)); };
  boost::mp11::mp_for_each<args>([=]<typename... A>(A&&... a) { (add(a), ...); });
}

template <typename Field>
void print_audio(dump_json::value obj)
{
  using type = typename Field::type;
  if constexpr(avnd::audio_sample_port<float, type>)
  {
    obj["sample_format"] = "float";
    obj["port_format"] = "sample";
  }
  else if constexpr(avnd::audio_sample_port<double, type>)
  {
    obj["sample_format"] = "double";
    obj["port_format"] = "sample";
  }
  else if constexpr(avnd::mono_array_sample_port<float, type>)
  {
    obj["sample_format"] = "float";
    obj["port_format"] = "channel";
  }
  else if constexpr(avnd::mono_array_sample_port<double, type>)
  {
    obj["sample_format"] = "double";
    obj["port_format"] = "channel";
  }
  else if constexpr(avnd::poly_array_sample_port<float, type>)
  {
    obj["sample_format"] = "float";
    obj["port_format"] = "bus";
  }
  else if constexpr(avnd::poly_array_sample_port<double, type>)
  {
    obj["sample_format"] = "double";
    obj["port_format"] = "bus";
  }
  else if constexpr(avnd::audio_frame_port<float, type>)
  {
    obj["sample_format"] = "float";
    obj["port_format"] = "frame";
  }
  else if constexpr(avnd::audio_frame_port<double, type>)
  {
    obj["sample_format"] = "double";
    obj["port_format"] = "frame";
  }
  else
  {
    obj["port_format"] = "unknown";
  }
}

template <typename Field>
void print_buffer(dump_json::value obj)
{
  using type = typename Field::type;
  if constexpr(avnd::gpu_buffer_port<type>)
    obj["storage"] = "gpu";
  else if constexpr(avnd::cpu_typed_buffer_port<type>)
    obj["storage"] = "cpu_typed";
  else if constexpr(avnd::cpu_raw_buffer_port<type>)
    obj["storage"] = "cpu_raw";
  else
    obj["storage"] = "unknown";
}

template <typename Field>
void print_tensor(dump_json::value obj)
{
  using type = typename Field::type;
  using val = std::decay_t<decltype(std::declval<type&>().value)>;
  if constexpr(avnd::view_tensor_like<val>)
    obj["container"] = "view";
  else if constexpr(avnd::resizable_tensor_like<val>)
    obj["container"] = "resizable";
  else if constexpr(avnd::mutable_tensor_like<val>)
    obj["container"] = "mutable";
  else if constexpr(avnd::dynamic_tensor_like<val>)
    obj["container"] = "dynamic";
  else
    obj["container"] = "unknown";

  if constexpr(avnd::tensor_like<val>)
  {
    using el = avnd::tensor_element<val>;
    if constexpr(avnd::has_avnd_dtype<el>)
    {
      constexpr auto d = avnd::dtype_of<el>;
      obj["dtype_code"] = (int)d.code;
      obj["dtype_bits"] = (int)d.bits;
    }
  }
}

template <typename Field>
void print_geometry(dump_json::value obj)
{
  using type = typename Field::type;
  if constexpr(avnd::dynamic_geometry_type<type>)
    obj["geometry"] = "dynamic";
  else if constexpr(avnd::static_geometry_type<type>)
    obj["geometry"] = "static";
  else
    obj["geometry"] = "mesh"; // geometry carried in a `.mesh` member
}

template <typename Field>
void print_parameter(dump_json::value obj)
{
  using type = typename Field::type;
  obj["value_type"] = parameter_value_type(Field{});

  if constexpr(avnd::control_port<type>)
    obj["control"] = true;
  else
    obj["value_port"] = true;

  if constexpr(avnd::sample_accurate_parameter_port<type>)
  {
    if constexpr(avnd::linear_sample_accurate_parameter_port<type>)
    {
      obj["sample_accurate"] = "linear";
    }
    else if constexpr(avnd::span_sample_accurate_parameter_port<type>)
    {
      obj["sample_accurate"] = "span";
    }
    else if constexpr(avnd::dynamic_sample_accurate_parameter_port<type>)
    {
      obj["sample_accurate"] = "dynamic";
    }
  }

  if constexpr(avnd::parameter_with_minmax_range<type>)
  {
    auto range = obj["range"];
    static constexpr auto ctl = avnd::get_range<type>();
    auto put = [&](const char* k, auto v) {
      if constexpr(std::is_arithmetic_v<std::decay_t<decltype(v)>>)
        range[k] = v;
    };
    if constexpr(requires { ctl.min; })
      put("min", ctl.min);
    if constexpr(requires { ctl.max; })
      put("max", ctl.max);
    if constexpr(requires { ctl.init; })
      put("init", ctl.init);
  }
  else if constexpr(avnd::enum_parameter<type>)
  {
    obj["choices"] = avnd::get_enum_choices<type>();
  }
  else
  {
    if_possible(obj["default"] = type{}.value);
  }

  if constexpr(avnd::has_widget<type>)
  {
    const auto& p1  = avnd::get_widget<type>();
    const auto& p2 = p1.name();
    obj["widget"] = p2;
  }

  if constexpr(avnd::smooth_parameter_port<type>)
  {
    obj["smooth"] = true;
  }
  if constexpr(avnd::has_mapper<type>)
  {
    obj["mapper"] = true;
  }
}

template <typename T>
bool setup_generated_audio_port(dump_json::value obj)
{
  obj["type"] = "audio";

  if constexpr(avnd::mono_per_sample_arg_processor<float, T>)
  {
    obj["audio"]["sample_format"] = "float";
    obj["audio"]["port_format"] = "sample";
    return true;
  }
  else if constexpr(avnd::mono_per_sample_arg_processor<double, T>)
  {
    obj["audio"]["sample_format"] = "double";
    obj["audio"]["port_format"] = "sample";
    return true;
  }
  else if constexpr(avnd::monophonic_arg_audio_effect<float, T>)
  {
    obj["audio"]["sample_format"] = "float";
    obj["audio"]["port_format"] = "channel";
    return true;
  }
  else if constexpr(avnd::monophonic_arg_audio_effect<double, T>)
  {
    obj["audio"]["sample_format"] = "double";
    obj["audio"]["port_format"] = "channel";
    return true;
  }
  else if constexpr(avnd::polyphonic_arg_audio_effect<float, T>)
  {
    obj["audio"]["sample_format"] = "float";
    obj["audio"]["port_format"] = "bus";
    return true;
  }
  else if constexpr(avnd::polyphonic_arg_audio_effect<double, T>)
  {
    obj["audio"]["sample_format"] = "double";
    obj["audio"]["port_format"] = "bus";
    return true;
  }
  return false;
}

template <typename T>
void print_generated_inputs(dump_json::value arr)
{
  // Add the implicit first port for audio I/O
  auto obj = arr.owner->make_node();
  obj["name"] = "audio in";
  obj["description"] = "audio in";
  if(setup_generated_audio_port<T>(obj))
    arr.push_back(obj);
}

template <typename T>
void print_generated_outputs(dump_json::value arr)
{
  // Add the implicit first port for audio I/O
  auto obj = arr.owner->make_node();
  obj["name"] = "audio out";
  obj["description"] = "audio out";
  if(setup_generated_audio_port<T>(obj))
    arr.push_back(obj);
}

template <typename Field>
void emit_port(Field wrap, dump_json::value obj)
{
  using type = typename Field::type;
  print_metadatas<type>(obj);
  obj["type"] = port_type(wrap);

  if constexpr(avnd::tensor_port<type>)
    print_tensor<Field>(obj["tensor"]);
  else if constexpr(avnd::buffer_port<type>)
    print_buffer<Field>(obj["buffer"]);
  else if constexpr(avnd::geometry_port<type>)
    print_geometry<Field>(obj["geometry"]);
  else if constexpr(avnd::parameter_port<type>)
    print_parameter<Field>(obj["parameter"]);
  else if constexpr(avnd::audio_port<type>)
    print_audio<Field>(obj["audio"]);
  else if constexpr(avnd::callback<type>)
    print_callback<Field>(obj["callback"]);
}

template <typename Processor>
void print_inputs(dump_json::value arr)
{
  print_generated_inputs<Processor>(arr);

  avnd::input_introspection<Processor>::for_all([&]<typename Field>(Field wrap) {
    auto obj = arr.owner->make_node();
    emit_port(wrap, obj);
    arr.push_back(obj);
  });
}

template <typename Processor>
void print_outputs(dump_json::value arr)
{
  print_generated_outputs<Processor>(arr);

  avnd::output_introspection<Processor>::for_all([&]<typename Field>(Field wrap) {
    auto obj = arr.owner->make_node();
    emit_port(wrap, obj);
    arr.push_back(obj);
  });
}

template <typename refl>
void print_arguments(refl, dump_json::value obj)
{
  using args = typename refl::arguments;
  auto arr = obj["arguments"];
  arr.ensure_array();
  boost::mp11::mp_for_each<boost::mp11::mp_iota_c<refl::count>>([&](auto I) {
    using arg_type = boost::mp11::mp_at_c<args, I>;
    arr.push_back(type_name<arg_type>());
  });

  using ret = typename refl::return_type;
  obj["return"] = type_name<ret>();
}

template <typename Processor, typename type>
void print_message(dump_json::value obj)
{
  using refl_type = decltype(avnd::message_function_reflection<type>());
  if constexpr(!std::is_same_v<refl_type, void>)
  {
    if constexpr(requires(Processor p, std::vector<std::variant<float, std::string>> v) {
                   type::func()(p, v);
                 })
    {
      obj["type"] = "type-generic";
    }
    else
    {
      if constexpr(avnd::stateless_message<type>)
      {
        if constexpr(std::is_member_function_pointer_v<decltype(type::func())>)
          obj["type"] = "member-function-pointer";
        else
          obj["type"] = "free-function";
      }
      else if constexpr(avnd::stateful_message<type>)
      {
        obj["type"] = "returns-function-object";
      }
      else if constexpr(avnd::stdfunc_message<type>)
      {
        obj["type"] = "std::function";
      }
      else if constexpr(avnd::function_object_message<type>)
      {
        obj["type"] = "function-object";
      }
    }
    print_arguments(avnd::message_function_reflection<type>(), obj);
  }
}

template <typename Processor>
void print_messages(dump_json::value arr)
{
  avnd::messages_introspection<Processor>::for_all([&]<typename Field>(Field wrap) {
    using type = typename Field::type;

    auto obj = arr.owner->make_node();
    print_metadatas<type>(obj);
    print_message<Processor, type>(obj);
    arr.push_back(obj);
  });
}

template <typename Processor>
void dump(std::string_view path)
{
  dump_json::document doc;
  auto obj = doc.root();
  print_metadatas<Processor>(obj["metadatas"]);
  print_supported_features<Processor>(obj["features"]);

  print_processor_types<Processor>();

  if constexpr(avnd::input_introspection<Processor>::size > 0)
  {
    print_inputs<Processor>(obj["inputs"]);
  }
  if constexpr(avnd::output_introspection<Processor>::size > 0)
  {
    print_outputs<Processor>(obj["outputs"]);
  }
  if constexpr(avnd::messages_introspection<Processor>::size > 0)
  {
    print_messages<Processor>(obj["messages"]);
  }

  auto res = doc.dump();

  if(path.empty())
  {
    std::cout << res << std::endl;
  }
  else
  {
    std::ofstream outf(path.data(), std::ios::binary);
    outf << res << std::endl;
  }
}
}
