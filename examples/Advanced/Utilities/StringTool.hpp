#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ao
{
namespace string_detail
{
inline constexpr int byte_limit = 1024 * 1024;
inline constexpr int part_limit = 65536;

// Strict UTF-8 scalar validation. The existing UTF-16 conversion helper is not
// a validator: it permits overlong encodings and surrogate code points.
inline bool valid_utf8(std::string_view text, std::size_t* count = nullptr)
{
  std::size_t n = 0;
  for(std::size_t i = 0; i < text.size(); ++n)
  {
    const auto lead = static_cast<unsigned char>(text[i++]);
    if(lead < 0x80)
      continue;
    int extra;
    std::uint32_t cp;
    std::uint32_t minimum;
    if(lead >= 0xC2 && lead <= 0xDF)
    {
      extra = 1;
      cp = lead & 0x1F;
      minimum = 0x80;
    }
    else if(lead >= 0xE0 && lead <= 0xEF)
    {
      extra = 2;
      cp = lead & 0x0F;
      minimum = 0x800;
    }
    else if(lead >= 0xF0 && lead <= 0xF4)
    {
      extra = 3;
      cp = lead & 0x07;
      minimum = 0x10000;
    }
    else
      return false;
    if(text.size() - i < static_cast<std::size_t>(extra))
      return false;
    for(int k = 0; k < extra; ++k)
    {
      const auto b = static_cast<unsigned char>(text[i++]);
      if((b & 0xC0) != 0x80)
        return false;
      cp = (cp << 6) | (b & 0x3F);
    }
    if(cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
      return false;
  }
  if(count)
    *count = n;
  return true;
}

// Only called on already validated UTF-8.
inline std::size_t width(unsigned char lead)
{
  return lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
}

inline std::size_t offset(std::string_view text, std::size_t codepoints)
{
  std::size_t pos = 0;
  while(codepoints-- && pos < text.size())
    pos += width(static_cast<unsigned char>(text[pos]));
  return pos;
}

inline bool ascii_space(char c)
{
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

inline bool valid_limit(int limit)
{
  return limit >= 1 && limit <= byte_limit;
}
}

enum class StringTrim
{
  None,
  Both,
  Left,
  Right
};
enum class StringCase
{
  Unchanged,
  ASCII_Lower,
  ASCII_Upper
};

/**
 * Message-triggered, cold controls. Chain order:
 * ASCII trim -> codepoint slice -> literal replacement -> ASCII case ->
 * codepoint reverse -> signed left rotation -> repeat -> left/right padding ->
 * maximum-length truncation -> minimum-length right padding.
 * Slice starts are clamped; negative starts count from the end; length -1
 * means the remainder. Rotation wraps, including negative (right) rotations.
 * Padding counts copies of exactly one Unicode scalar, not a target width.
 * Min length 0 and Max length -1 disable their limits. Active minimum greater
 * than active maximum is an error, even when a particular input would fit.
 * Codepoints are NOT graphemes: combining marks / emoji sequences can separate.
 * All text is strict UTF-8, including embedded NUL. No normalization is done.
 * Max bytes bounds input, text options and every intermediate/output string.
 * On failure emit only Error; on success clear Error before emitting Text.
 */
struct StringTool
{
  halp_meta(name, "String tool")
  halp_meta(c_name, "string_tool")
  halp_meta(category, "Control/Strings")
  halp_meta(author, "ossia team")
  halp_meta(
      description,
      "UTF-8 codepoint trim, slice, replace, case, reverse, rotate, repeat, pad "
      "and length limits; ASCII trim/case")
  halp_meta(uuid, "a7de81b5-6e90-47c8-8f2b-9023c0a16e54")

  struct ins
  {
    halp::enum_t<StringTrim, "ASCII trim"> trim;
    halp::toggle<"Slice"> slice;
    struct : halp::spinbox_i32<"Start", halp::range{-1048576, 1048576, 0}>
    {
      halp_meta(
          description,
          "Slice start in Unicode codepoints; negative counts from the end.")
    } start;
    struct : halp::spinbox_i32<"Length", halp::range{-1, 1048576, -1}>
    {
      halp_meta(
          description, "Slice length in Unicode codepoints; -1 keeps the remainder.")
    } length;
    halp::toggle<"Replace"> replace;
    struct : halp::lineedit<"Find", "">
    {
      halp_meta(description, "Literal text to replace; not a regular expression.")
    } find;
    halp::lineedit<"Replacement", ""> replacement;
    halp::enum_t<StringCase, "Case"> casing;
    struct : halp::toggle<"Reverse">
    {
      halp_meta(description, "Reverse Unicode codepoints, not grapheme clusters.")
    } reverse;
    struct : halp::spinbox_i32<"Rotate left", halp::range{-1048576, 1048576, 0}>
    {
      halp_meta(description, "Rotate by Unicode codepoints; negative rotates right.")
    } rotate;
    halp::spinbox_i32<"Repeat", halp::range{0, 1048576, 1}> repeat;
    struct : halp::spinbox_i32<"Pad left", halp::range{0, 1048576, 0}>
    {
      halp_meta(description, "Number of copies of Pad character to prepend.")
    } pad_left;
    struct : halp::spinbox_i32<"Pad right", halp::range{0, 1048576, 0}>
    {
      halp_meta(description, "Number of copies of Pad character to append.")
    } pad_right;
    halp::lineedit<"Pad character", " "> pad;
    struct : halp::spinbox_i32<"Min length", halp::range{0, 1048576, 0}>
    {
      halp_meta(
          description,
          "Minimum final Unicode codepoint count; 0 disables. Pads on the right after "
          "truncation.")
    } min_length;
    struct : halp::spinbox_i32<"Max length", halp::range{-1, 1048576, -1}>
    {
      halp_meta(
          description,
          "Maximum final Unicode codepoint count; -1 disables, 0 produces empty text. "
          "Truncates after existing transformations.")
    } max_length;
    struct : halp::lineedit<"Min length pad", " ">
    {
      halp_meta(
          description,
          "Exactly one Unicode codepoint, appended as needed to reach Min length.")
    } min_pad;
    halp::spinbox_i32<"Max bytes", halp::range{1, 1048576, 1048576}> max_bytes;
  } inputs;

  struct messages
  {
    struct message
    {
      halp_meta(name, "Text")
      void operator()(StringTool& self, const std::string& text) { self.process(text); }
    } text;
  };

  struct outs
  {
    halp::callback<"Text", std::string> text;
    halp::callback<"Error", std::string> error;
  } outputs;

  void process(std::string_view source)
  {
    using namespace string_detail;
    const auto fail = [this](const char* message) { outputs.error(message); };
    if(!valid_limit(inputs.max_bytes))
      return fail("Max bytes must be between 1 and 1048576");
    const std::size_t limit = inputs.max_bytes.value;
    if(source.size() > limit)
      return fail("Input exceeds byte limit");
    if(!valid_utf8(source))
      return fail("Input is not valid UTF-8");
    if(inputs.repeat < 0 || inputs.pad_left < 0 || inputs.pad_right < 0
       || (inputs.slice && inputs.length < -1))
      return fail("Invalid negative repeat, padding or slice length");
    if(inputs.min_length < 0 || inputs.min_length > byte_limit || inputs.max_length < -1
       || inputs.max_length > byte_limit)
      return fail("Invalid minimum or maximum length");
    if(inputs.max_length >= 0 && inputs.min_length > inputs.max_length.value)
      return fail("Min length exceeds Max length");
    if(std::size_t(inputs.min_length.value) > limit)
      return fail("Min length cannot fit within byte limit");
    switch(inputs.trim.value)
    {
      case StringTrim::None:
        break;
      case StringTrim::Both:
      case StringTrim::Left:
      case StringTrim::Right:
        if(inputs.trim != StringTrim::Right)
          while(!source.empty() && ascii_space(source.front()))
            source.remove_prefix(1);
        if(inputs.trim != StringTrim::Left)
          while(!source.empty() && ascii_space(source.back()))
            source.remove_suffix(1);
        break;
      default:
        return fail("Invalid trim mode");
    }
    if(inputs.slice)
    {
      std::size_t count{};
      valid_utf8(source, &count);
      const auto start = std::clamp<std::int64_t>(
          inputs.start < 0 ? std::int64_t(count) + inputs.start.value
                           : inputs.start.value,
          0, count);
      source.remove_prefix(offset(source, start));
      if(inputs.length >= 0)
        source = source.substr(0, offset(source, inputs.length.value));
    }
    std::string text;
    if(inputs.replace)
    {
      const auto& find = inputs.find.value;
      const auto& replacement = inputs.replacement.value;
      if(find.empty())
        return fail("Literal search must not be empty");
      if(find.size() > limit || replacement.size() > limit)
        return fail("Replacement options exceed byte limit");
      if(!valid_utf8(find) || !valid_utf8(replacement))
        return fail("Replacement options are not valid UTF-8");
      // Non-overlapping left-to-right matches in the original slice. Inserted
      // text is never searched again (including when it contains the needle).
      std::size_t matches = 0;
      for(std::size_t pos = 0; (pos = source.find(find, pos)) != source.npos;
          pos += find.size())
        ++matches;
      std::size_t size = source.size() - matches * find.size();
      if(!replacement.empty() && matches > (limit - size) / replacement.size())
        return fail("Replacement exceeds byte limit");
      size += matches * replacement.size();
      text.reserve(size);
      std::size_t pos = 0;
      for(std::size_t match; (match = source.find(find, pos)) != source.npos;)
      {
        text.append(source.substr(pos, match - pos));
        text.append(replacement);
        pos = match + find.size();
      }
      text.append(source.substr(pos));
    }
    else
      text.assign(source);

    switch(inputs.casing.value)
    {
      case StringCase::Unchanged:
        break;
      case StringCase::ASCII_Lower:
        for(char& c : text)
          if(c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
        break;
      case StringCase::ASCII_Upper:
        for(char& c : text)
          if(c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        break;
      default:
        return fail("Invalid case mode");
    }
    if(inputs.reverse)
    {
      for(std::size_t pos = 0; pos < text.size();)
      {
        const auto end = pos + width(static_cast<unsigned char>(text[pos]));
        std::reverse(text.begin() + pos, text.begin() + end);
        pos = end;
      }
      std::reverse(text.begin(), text.end());
    }
    if(inputs.rotate != 0 && !text.empty())
    {
      std::size_t count{};
      valid_utf8(text, &count);
      std::int64_t rotation = std::int64_t(inputs.rotate.value) % std::int64_t(count);
      if(rotation < 0)
        rotation += count;
      std::rotate(text.begin(), text.begin() + offset(text, rotation), text.end());
    }
    const std::size_t repeat = inputs.repeat.value;
    if(!text.empty() && repeat > limit / text.size())
      return fail("Repeat exceeds byte limit");
    const auto original_size = text.size();
    text.resize(original_size * repeat);
    if(original_size)
      for(std::size_t pos = original_size; pos < text.size(); pos += original_size)
        std::copy_n(text.begin(), original_size, text.begin() + pos);

    const std::size_t padding
        = std::size_t(inputs.pad_left.value) + inputs.pad_right.value;
    if(padding)
    {
      std::size_t count{};
      const auto& pad = inputs.pad.value;
      if(pad.size() > limit || !valid_utf8(pad, &count) || count != 1)
        return fail("Padding must be exactly one UTF-8 codepoint within the byte limit");
      if(padding > (limit - text.size()) / pad.size())
        return fail("Padding exceeds byte limit");
      const auto left_bytes = std::size_t(inputs.pad_left.value) * pad.size();
      const auto body_size = text.size();
      text.resize(body_size + padding * pad.size());
      if(left_bytes && body_size)
        std::move_backward(
            text.begin(), text.begin() + body_size,
            text.begin() + left_bytes + body_size);
      for(std::size_t pos = 0; pos < left_bytes; pos += pad.size())
        std::copy(pad.begin(), pad.end(), text.begin() + pos);
      for(std::size_t pos = left_bytes + body_size; pos < text.size(); pos += pad.size())
        std::copy(pad.begin(), pad.end(), text.begin() + pos);
    }
    if(inputs.max_length >= 0)
      text.resize(offset(text, inputs.max_length.value));
    if(inputs.min_length > 0)
    {
      std::size_t count{};
      valid_utf8(text, &count);
      if(count < std::size_t(inputs.min_length.value))
      {
        std::size_t pad_count{};
        const auto& pad = inputs.min_pad.value;
        if(pad.size() > limit || !valid_utf8(pad, &pad_count) || pad_count != 1)
          return fail(
              "Min length pad must be exactly one UTF-8 codepoint within the byte "
              "limit");
        const auto missing = std::size_t(inputs.min_length.value) - count;
        if(missing > (limit - text.size()) / pad.size())
          return fail("Min length padding exceeds byte limit");
        const auto body_size = text.size();
        text.resize(body_size + missing * pad.size());
        for(std::size_t pos = body_size; pos < text.size(); pos += pad.size())
          std::copy(pad.begin(), pad.end(), text.begin() + pos);
      }
    }
    outputs.error(std::string{});
    outputs.text(std::move(text));
  }
};

/** Literal full-string delimiter; empty delimiter splits Unicode codepoints.
 * Keep empty preserves leading, consecutive and trailing empty fields. Empty
 * input yields one empty field with a nonempty delimiter, none in codepoint
 * mode. No quoting/CSV parsing. Max parts rejects overflow, never truncates.
 * Messages trigger processing; controls are cold. Error contract as StringTool.
 */
struct StringSplit
{
  halp_meta(name, "Split string")
  halp_meta(c_name, "string_split")
  halp_meta(category, "Control/Strings")
  halp_meta(author, "ossia team")
  halp_meta(
      description,
      "Split UTF-8 by a literal delimiter; empty delimiter splits codepoints")
  halp_meta(uuid, "5e3a285a-37c6-4e38-97b3-03e216843409")

  struct ins
  {
    struct : halp::lineedit<"Delimiter", ",">
    {
      halp_meta(description, "Literal delimiter; empty splits Unicode codepoints.")
    } delimiter;
    halp::toggle<"Keep empty", halp::default_on_toggle> keep_empty;
    halp::spinbox_i32<"Max parts", halp::range{1, 65536, 65536}> max_parts;
    halp::spinbox_i32<"Max bytes", halp::range{1, 1048576, 1048576}> max_bytes;
  } inputs;
  struct messages
  {
    struct message
    {
      halp_meta(name, "Text")
      void operator()(StringSplit& self, const std::string& text) { self.process(text); }
    } text;
  };
  struct outs
  {
    halp::callback<"Parts", std::vector<std::string>> parts;
    halp::callback<"Error", std::string> error;
  } outputs;

  void process(std::string_view text)
  {
    using namespace string_detail;
    const auto fail = [this](const char* message) { outputs.error(message); };
    if(!valid_limit(inputs.max_bytes) || inputs.max_parts < 1
       || inputs.max_parts > part_limit)
      return fail("Invalid byte or part limit");
    const auto& delimiter = inputs.delimiter.value;
    const std::size_t limit = inputs.max_bytes.value;
    if(text.size() > limit || delimiter.size() > limit)
      return fail("Input or delimiter exceeds byte limit");
    if(!valid_utf8(text) || !valid_utf8(delimiter))
      return fail("Input or delimiter is not valid UTF-8");
    std::vector<std::string> parts;
    const auto append = [&](std::string_view part) {
      if(part.empty() && !inputs.keep_empty)
        return true;
      if(parts.size() >= std::size_t(inputs.max_parts.value))
        return false;
      parts.emplace_back(part);
      return true;
    };
    if(delimiter.empty())
    {
      while(!text.empty())
      {
        const auto n = width(static_cast<unsigned char>(text.front()));
        if(!append(text.substr(0, n)))
          return fail("Split exceeds part limit");
        text.remove_prefix(n);
      }
    }
    else
    {
      for(std::size_t pos; (pos = text.find(delimiter)) != text.npos;)
      {
        if(!append(text.substr(0, pos)))
          return fail("Split exceeds part limit");
        text.remove_prefix(pos + delimiter.size());
      }
      if(!append(text))
        return fail("Split exceeds part limit");
    }
    outputs.error(std::string{});
    outputs.parts(std::move(parts));
  }
};

/** Join UTF-8 string lists without implicit number coercion or quoting. Empty
 * fields are retained; separators go only between fields. Hard part limit
 * bounds processing even for lists of empty strings. Error contract as above.
 */
struct StringJoin
{
  halp_meta(name, "Join strings")
  halp_meta(c_name, "string_join")
  halp_meta(category, "Control/Strings")
  halp_meta(author, "ossia team")
  halp_meta(description, "Join UTF-8 string lists with a literal separator")
  halp_meta(uuid, "ebae143d-ff33-422c-afdc-150c7738db1f")
  struct ins
  {
    halp::lineedit<"Separator", ","> separator;
    halp::spinbox_i32<"Max bytes", halp::range{1, 1048576, 1048576}> max_bytes;
  } inputs;
  struct messages
  {
    struct message
    {
      halp_meta(name, "Parts")
      void operator()(StringJoin& self, const std::vector<std::string>& parts)
      {
        self.process(parts);
      }
    } parts;
  };
  struct outs
  {
    halp::callback<"Text", std::string> text;
    halp::callback<"Error", std::string> error;
  } outputs;

  void process(const std::vector<std::string>& parts)
  {
    using namespace string_detail;
    const auto fail = [this](const char* message) { outputs.error(message); };
    if(!valid_limit(inputs.max_bytes))
      return fail("Invalid byte limit");
    if(parts.size() > part_limit)
      return fail("Input exceeds part limit");
    const std::size_t limit = inputs.max_bytes.value;
    const auto& separator = inputs.separator.value;
    if(separator.size() > limit)
      return fail("Separator exceeds byte limit");
    if(!valid_utf8(separator))
      return fail("Separator is not valid UTF-8");
    std::size_t size = 0;
    if(parts.size() > 1)
    {
      if(separator.size() > limit / (parts.size() - 1))
        return fail("Join exceeds byte limit");
      size = separator.size() * (parts.size() - 1);
    }
    for(const auto& part : parts)
    {
      if(part.size() > limit - size)
        return fail("Join exceeds byte limit");
      if(!valid_utf8(part))
        return fail("A part is not valid UTF-8");
      size += part.size();
    }
    std::string text;
    text.reserve(size);
    for(std::size_t i = 0; i < parts.size(); ++i)
    {
      if(i)
        text.append(separator);
      text.append(parts[i]);
    }
    outputs.error(std::string{});
    outputs.text(std::move(text));
  }
};
}
