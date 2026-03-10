#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <halp/audio.hpp>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace uo
{
struct BufferToArray
{
public:
  halp_meta(name, "Buffer to array")
  halp_meta(c_name, "avnd_buffertoarray")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(description, "GPU buffer -> CPU buffer")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/buffer_to_array.html")
  halp_meta(author, "ossia score")
  halp_meta(uuid, "5274a895-8232-4e79-bc7a-50c27274bd10")
  enum Type
  {
    Float32,
    Float64,
    SInt32,
    UInt32,
    SInt16,
    UInt16,
    SInt8,
    UInt8,
  };

  enum Mode
  {
    FloatArray,
    IntArray,
    String
  };

  struct
  {
    halp::cpu_buffer_input<"Input"> main;
    halp::enum_t<Type, "Type"> type;
    halp::combobox_t<"Mode", Mode> mode;
  } inputs;

  struct
  {
    halp::val_port<
        "Output", std::variant<std::vector<float>, std::vector<int>, std::string>>
        main;
  } outputs;

  template <typename T>
  void process(auto& v)
  {
    v.clear();
    auto sz = inputs.main.buffer.byte_size;
    auto elements = sz / sizeof(T);
    auto ptr = (T*)inputs.main.buffer.raw_data;
    v.assign(ptr, ptr + elements);
  }

  void do_process(auto& v)
  {
    switch(inputs.type)
    {
      case Type::Float32:
        return process<float>(v);
      case Type::Float64:
        return process<double>(v);
      case Type::SInt32:
        return process<int32_t>(v);
      case Type::UInt32:
        return process<uint32_t>(v);
      case Type::SInt16:
        return process<int16_t>(v);
      case Type::UInt16:
        return process<uint16_t>(v);
      case Type::SInt8:
        return process<int8_t>(v);
      case Type::UInt8:
        return process<uint8_t>(v);
    }
  }

  void operator()() noexcept
  {
    switch(inputs.mode)
    {
      case Mode::FloatArray: {
        if(auto ptr = std::get_if<std::vector<float>>(&outputs.main.value))
          return do_process(*ptr);
        else
          return do_process(outputs.main.value.emplace<0>());
        break;
      }
      case Mode::IntArray: {
        if(auto ptr = std::get_if<std::vector<int>>(&outputs.main.value))
          return do_process(*ptr);
        else
          return do_process(outputs.main.value.emplace<1>());
        break;
      }
      case Mode::String: {
        if(auto ptr = std::get_if<std::string>(&outputs.main.value))
          return do_process(*ptr);
        else
          return do_process(outputs.main.value.emplace<2>());
        break;
      }
    }
  }
};
}
