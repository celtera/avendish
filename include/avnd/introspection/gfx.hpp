#pragma once
#include <avnd/concepts/gfx.hpp>
#include <avnd/common/inline.hpp>

namespace avnd
{
AVND_INLINE auto get_bytesize(const avnd::cpu_raw_buffer auto& field)
{
  return field.byte_size;
}

AVND_INLINE auto get_bytesize(const avnd::cpu_typed_buffer auto& field)
{
  return field.element_count * sizeof(std::decay_t<decltype(field.elements[0])>);
}

AVND_INLINE auto get_bytes(const avnd::cpu_raw_buffer auto& field)
{
  return field.raw_data;
}

AVND_INLINE auto get_bytes(const avnd::cpu_typed_buffer auto& field)
{
  return reinterpret_cast<const unsigned char*>(field.elements);
}

AVND_INLINE auto get_bytes(avnd::cpu_raw_buffer auto& field)
{
  return field.raw_data;
}

AVND_INLINE auto get_bytes(avnd::cpu_typed_buffer auto& field)
{
  return reinterpret_cast<unsigned char*>(field.elements);
}

}
