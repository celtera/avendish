#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace uo
{
struct ArrayToBuffer
{
public:
  halp_meta(name, "Array to buffer")
  halp_meta(c_name, "avnd_arraytobuffer")
  halp_meta(category, "Visuals")
  halp_meta(description, "CPU buffer -> GPU buffer")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/array_to_buffer.html")
  halp_meta(author, "ossia score")
  halp_meta(uuid, "3127c11b-61d3-4d5e-a896-8b56e628dfbc")

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

  using input_value_type = float;

  struct
  {
    halp::val_port<"Input", std::vector<input_value_type>> main;
    halp::enum_t<Type, "Type"> type;
  } inputs;

  struct
  {
    halp::cpu_buffer_output<"Output"> main;
  } outputs;

  template <typename T>
  void process()
  {
    if constexpr(std::is_same_v<T, input_value_type>)
    {
      // Direct reuse of the buffer
      outputs.main.buffer.raw_data = (unsigned char*)inputs.main.value.data();
      outputs.main.buffer.byte_size = inputs.main.value.size() * sizeof(float);
      outputs.main.buffer.byte_offset = 0;
      outputs.main.buffer.changed = true;
    }
    else
    {
      // Allocation and copy :'(
      auto span = outputs.main.create<T>(inputs.main.value.size());
      std::copy_n(inputs.main.value.data(), inputs.main.value.size(), span.data());
      outputs.main.buffer.changed = true;
    }
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
