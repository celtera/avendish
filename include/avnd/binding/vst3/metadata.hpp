#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <base/source/fstring.h>
#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/vst/vsttypes.h>

#include <cinttypes>
#include <cstdint>
#include <exception>
#include <string_view>

namespace stv3
{
using TBool = Steinberg::TBool;
using TChar = Steinberg::Vst::TChar;
using int8 = Steinberg::int8;
using int16 = Steinberg::int16;
using int32 = Steinberg::int32;
using uint8 = Steinberg::uint8;
using uint16 = Steinberg::uint16;
using uint32 = Steinberg::uint32;
using tresult = Steinberg::tresult;
struct UnionID
{
  union
  {
    uint32_t raw[4];
    Steinberg::TUID tuid;
  };
};

static consteval uint8_t hex_to_int(char c1)
{
  switch(c1)
  {
    case '0':
      return 0;
    case '1':
      return 1;
    case '2':
      return 2;
    case '3':
      return 3;
    case '4':
      return 4;
    case '5':
      return 5;
    case '6':
      return 6;
    case '7':
      return 7;
    case '8':
      return 8;
    case '9':
      return 9;
    case 'a':
    case 'A':
      return 10;
    case 'b':
    case 'B':
      return 11;
    case 'c':
    case 'C':
      return 12;
    case 'd':
    case 'D':
      return 13;
    case 'e':
    case 'E':
      return 14;
    case 'f':
    case 'F':
      return 15;
    default:
      std::terminate();
  }
}

static consteval uint8_t decode_hex(char c1, char c2)
{
  return (hex_to_int(c1) << 4) + hex_to_int(c2);
}

// Note: it's likely that the endianness here isn't good because
// I just wanted to have something that works quickly
template <typename T>
static consteval UnionID component_uuid_for_type()
{
  constexpr std::string_view chars = T::uuid();
  static_assert(chars.length() == 36);

  UnionID id;
  id.tuid[0] = decode_hex(chars[0], chars[1]);
  id.tuid[1] = decode_hex(chars[2], chars[3]);
  id.tuid[2] = decode_hex(chars[4], chars[5]);
  id.tuid[3] = decode_hex(chars[6], chars[7]);

  id.tuid[4] = decode_hex(chars[9], chars[10]);
  id.tuid[5] = decode_hex(chars[11], chars[12]);
  id.tuid[6] = decode_hex(chars[14], chars[15]);
  id.tuid[7] = decode_hex(chars[16], chars[17]);

  id.tuid[8] = decode_hex(chars[19], chars[20]);
  id.tuid[9] = decode_hex(chars[21], chars[22]);
  id.tuid[10] = decode_hex(chars[24], chars[25]);
  id.tuid[11] = decode_hex(chars[26], chars[27]);

  id.tuid[12] = decode_hex(chars[28], chars[29]);
  id.tuid[13] = decode_hex(chars[30], chars[31]);
  id.tuid[14] = decode_hex(chars[32], chars[33]);
  id.tuid[15] = decode_hex(chars[34], chars[35]);

  return id;
}

template <typename T>
static consteval UnionID controller_uuid_for_type()
{
  UnionID component_id = component_uuid_for_type<T>();

  // We just swap the values to get a new uuid...
  constexpr auto exch = [](char& a, char& b) consteval
  {
    char tmp = a;
    a = b;
    b = tmp;
  };
  exch(component_id.tuid[0], component_id.tuid[15]);
  exch(component_id.tuid[1], component_id.tuid[14]);
  exch(component_id.tuid[2], component_id.tuid[13]);
  exch(component_id.tuid[3], component_id.tuid[12]);

  exch(component_id.tuid[4], component_id.tuid[12]);
  exch(component_id.tuid[5], component_id.tuid[11]);
  exch(component_id.tuid[6], component_id.tuid[10]);
  exch(component_id.tuid[7], component_id.tuid[9]);

  return component_id;
}

#define INIT_FROM_TUID(ID)                                                              \
  {                                                                                     \
    ID.tuid[0], ID.tuid[1], ID.tuid[2], ID.tuid[3], ID.tuid[4], ID.tuid[5], ID.tuid[6], \
        ID.tuid[7], ID.tuid[8], ID.tuid[9], ID.tuid[10], ID.tuid[11], ID.tuid[12],      \
        ID.tuid[13], ID.tuid[14], ID.tuid[15]                                           \
  }

#define TUID_FROM_UNION(ID)                                                             \
  {                                                                                     \
    ID.tuid[3], ID.tuid[2], ID.tuid[1], ID.tuid[0], ID.tuid[7], ID.tuid[6], ID.tuid[5], \
        ID.tuid[4], ID.tuid[11], ID.tuid[10], ID.tuid[9], ID.tuid[8], ID.tuid[15],      \
        ID.tuid[14], ID.tuid[13], ID.tuid[12]                                           \
  }

}
