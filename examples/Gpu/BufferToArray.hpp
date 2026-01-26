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

  struct
  {
    halp::cpu_buffer_input<"Input"> main;
    halp::enum_t<Type, "Type"> type;
  } inputs;

  struct
  {
    halp::val_port<"Output", std::vector<float>> main;
  } outputs;

  template <typename T>
  void process()
  {
    auto sz = inputs.main.buffer.byte_size;
    auto elements = sz / sizeof(T);
    auto ptr = (T*)inputs.main.buffer.raw_data;
    outputs.main.value.assign(ptr, ptr + elements);
  }

  void operator()() noexcept
  {
    switch(inputs.type)
    {
      case Type::Float32:
        return process<float>();
      case Type::Float64:
        return process<double>();
      case Type::SInt32:
        return process<int32_t>();
      case Type::UInt32:
        return process<uint32_t>();
      case Type::SInt16:
        return process<int16_t>();
      case Type::UInt16:
        return process<uint16_t>();
      case Type::SInt8:
        return process<int8_t>();
      case Type::UInt8:
        return process<uint8_t>();
    }
  }
};
}
