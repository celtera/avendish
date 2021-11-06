#pragma once
#include <cstddef>


namespace avnd
{
namespace detail
{
// Adapted from
// https://codereview.stackexchange.com/questions/262626/utf-8-to-utf-16-char8-t-string-to-char16-t-string
constexpr bool utf8_trail_byte(char8_t const in, char32_t& out) noexcept
{
  if (in < 0x80 || 0xBF < in)
    return false;

  out = (out << 6) | (in & 0x3F);
  return true;
}

// Returns number of trailing bytes.
// -1 on illegal header bytes.
constexpr int utf8_header_byte(char8_t const in, char32_t& out) noexcept {
  if (in < 0x80) {  // ASCII
    out = in;
    return 0;
  }
  if (in < 0xC0) {  // not a header
    return -1;
  }
  if (in < 0xE0) {
    out = in & 0x1F;
    return 1;
  }
  if (in < 0xF0) {
    out = in & 0x0F;
    return 2;
  }
  if (in < 0xF8) {
    out = in & 0x7;
    return 3;
  }
  return -1;
}
}  // namespace detail

template<typename Char_T>
constexpr std::ptrdiff_t utf8_to_utf16(
    const Char_T* u8_begin,
    const Char_T* const u8_end,
    char16_t* u16out) noexcept {
  std::ptrdiff_t outstr_size = 0;
  while (u8_begin < u8_end)
  {
    char32_t code_point = 0;
    const auto byte_cnt = detail::utf8_header_byte(*u8_begin++, code_point);

    if (byte_cnt < 0 || byte_cnt > u8_end - u8_begin)
      return false;

    for (int i = 0; i < byte_cnt; ++i)
      if (!detail::utf8_trail_byte(*u8_begin++, code_point))
        return -1;

    if (code_point < 0xFFFF) {
      if (u16out)
        *u16out++ = static_cast<char16_t>(code_point);
      ++outstr_size;
    } else {
      if (u16out) {
        code_point -= 0x10000;
        *u16out++ = static_cast<char16_t>((code_point >> 10) + 0xD800);
        *u16out++ = static_cast<char16_t>((code_point & 0x3FF) + 0xDC00);
      }
      outstr_size += 2;
    }
  }
  return outstr_size;
}
}  // namespace utf_converter
