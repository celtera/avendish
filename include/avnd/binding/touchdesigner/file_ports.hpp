#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <avnd/binding/touchdesigner/helpers.hpp>
#include <avnd/common/mmap.hpp>
#include <avnd/concepts/all.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/port.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/soundfile_storage.hpp>

#include <CPlusPlus_Common.h>

#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(AVND_TD_USE_DRLIBS)
#if __has_include(<dr_wav.h>)
#include <dr_wav.h>
#endif
#if __has_include(<dr_flac.h>)
#include <dr_flac.h>
#endif
#if __has_include(<dr_mp3.h>)
#include <dr_mp3.h>
#endif
#endif

namespace touchdesigner
{

/**
 * File-family ports for TouchDesigner.
 *
 * file_port / soundfile_port / midifile_port are not parameter_port (no
 * `.value`) so they are invisible to parameter_setup/parameter_update. This
 * component handles them for every operator type (TOP/POP/CHOP/SOP/DAT):
 *
 *  - setup(impl, manager): appends one TD `File` parameter per port.
 *  - load(impl, inputs):   each cook, resolves the file path and, when it has
 *                          changed (or when the `file_watch` tag is set and the
 *                          file's mtime changed), (re)loads the contents into
 *                          the port and calls the port's optional update().
 *
 * The loaded bytes must outlive the cook: halp file/soundfile views hold
 * non-owning views (std::string_view / raw pointers), so the owning buffers
 * live here in per-port slots.
 */

namespace detail
{
// getParFilePath() returns UTF-8; build a filesystem::path that round-trips
// non-ASCII characters correctly (notably on Windows, where the narrow path
// constructor would otherwise misinterpret UTF-8).
inline std::filesystem::path fs_path_from_utf8(std::string_view s)
{
#if defined(_WIN32)
  return std::filesystem::path(
      std::u8string{reinterpret_cast<const char8_t*>(s.data()), s.size()});
#else
  return std::filesystem::path(s);
#endif
}

// ---- minimal, dependency-free WAV decoder (the common TD audio case) -------
struct decoded_audio
{
  std::vector<float> interleaved; // frames * channels
  int channels = 0;
  std::int64_t frames = 0;
  int rate = 0;
};

inline std::uint32_t rd_u32(const unsigned char* p) noexcept
{
  return (std::uint32_t)p[0] | ((std::uint32_t)p[1] << 8)
         | ((std::uint32_t)p[2] << 16) | ((std::uint32_t)p[3] << 24);
}
inline std::uint16_t rd_u16(const unsigned char* p) noexcept
{
  return (std::uint16_t)(p[0] | (p[1] << 8));
}

inline bool decode_wav(const unsigned char* data, std::size_t size, decoded_audio& out)
{
  if(size < 44)
    return false;
  if(std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0)
    return false;

  std::size_t pos = 12;
  std::uint16_t fmt = 0, channels = 0, bits = 0;
  std::uint32_t rate = 0;
  const unsigned char* pcm = nullptr;
  std::size_t pcm_size = 0;
  bool have_fmt = false;

  while(pos + 8 <= size)
  {
    const unsigned char* id = data + pos;
    std::uint32_t csz = rd_u32(data + pos + 4);
    pos += 8;
    if(pos + csz > size)
      csz = static_cast<std::uint32_t>(size - pos); // clamp truncated files

    if(std::memcmp(id, "fmt ", 4) == 0 && csz >= 16)
    {
      fmt = rd_u16(data + pos);
      channels = rd_u16(data + pos + 2);
      rate = rd_u32(data + pos + 4);
      bits = rd_u16(data + pos + 14);
      // WAVE_FORMAT_EXTENSIBLE: the real format tag is in the SubFormat GUID.
      if(fmt == 0xFFFE && csz >= 40)
        fmt = rd_u16(data + pos + 24);
      have_fmt = true;
    }
    else if(std::memcmp(id, "data", 4) == 0)
    {
      pcm = data + pos;
      pcm_size = csz;
    }
    pos += csz + (csz & 1u); // chunks are word-aligned
  }

  if(!have_fmt || !pcm || channels == 0 || rate == 0 || bits == 0)
    return false;

  const int bytes_per = bits / 8;
  const std::int64_t total = static_cast<std::int64_t>(pcm_size / bytes_per);
  const std::int64_t frames = total / channels;
  if(frames <= 0)
    return false;

  out.channels = channels;
  out.frames = frames;
  out.rate = static_cast<int>(rate);
  out.interleaved.resize(static_cast<std::size_t>(frames * channels));

  auto conv = [&](std::int64_t i) -> float {
    const unsigned char* s = pcm + i * bytes_per;
    if(fmt == 3) // IEEE float
    {
      if(bits == 32) { float v; std::memcpy(&v, s, 4); return v; }
      if(bits == 64) { double v; std::memcpy(&v, s, 8); return static_cast<float>(v); }
    }
    else // PCM integer
    {
      if(bits == 8)
        return (static_cast<int>(s[0]) - 128) / 128.0f; // 8-bit is unsigned
      if(bits == 16)
        return static_cast<std::int16_t>(s[0] | (s[1] << 8)) / 32768.0f;
      if(bits == 24)
      {
        std::int32_t v = s[0] | (s[1] << 8) | (s[2] << 16);
        if(v & 0x800000)
          v |= ~0xFFFFFF; // sign-extend
        return v / 8388608.0f;
      }
      if(bits == 32)
      {
        std::int32_t v;
        std::memcpy(&v, s, 4);
        return static_cast<float>(v / 2147483648.0);
      }
    }
    return 0.0f;
  };

  for(std::int64_t i = 0, n = frames * channels; i < n; ++i)
    out.interleaved[static_cast<std::size_t>(i)] = conv(i);
  return true;
}

inline bool read_whole_file(const std::string& path, std::vector<unsigned char>& buf)
{
  std::ifstream f(fs_path_from_utf8(path), std::ios::binary);
  if(!f)
    return false;
  f.seekg(0, std::ios::end);
  const auto sz = f.tellg();
  f.seekg(0, std::ios::beg);
  if(sz <= 0)
    return false;
  buf.resize(static_cast<std::size_t>(sz));
  f.read(reinterpret_cast<char*>(buf.data()), sz);
  return static_cast<bool>(f);
}

/**
 * Decode an audio file to interleaved float.
 *
 * Built-in support covers uncompressed WAV (PCM 8/16/24/32-bit and IEEE float
 * 32/64-bit) with no external dependency. For more formats (FLAC, MP3,
 * compressed WAV) define AVND_TD_USE_DRLIBS and make the matching dr_libs
 * header(s) available; the DR_*_IMPLEMENTATION macros must be defined in
 * exactly one translation unit of the plugin. When neither path can handle the
 * file, this returns false and the soundfile port is left empty.
 */
inline bool decode_audio_file(const std::string& path, decoded_audio& out)
{
#if defined(AVND_TD_USE_DRLIBS)
  std::string ext;
  if(auto dot = path.find_last_of('.'); dot != std::string::npos)
  {
    ext = path.substr(dot + 1);
    for(auto& c : ext)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

#if __has_include(<dr_wav.h>)
  if(ext == "wav")
  {
    unsigned int ch = 0, sr = 0;
    drwav_uint64 fr = 0;
    if(float* s = drwav_open_file_and_read_pcm_frames_f32(
           path.c_str(), &ch, &sr, &fr, nullptr))
    {
      out.channels = static_cast<int>(ch);
      out.frames = static_cast<std::int64_t>(fr);
      out.rate = static_cast<int>(sr);
      out.interleaved.assign(s, s + fr * ch);
      drwav_free(s, nullptr);
      return out.channels > 0 && out.frames > 0;
    }
  }
#endif
#if __has_include(<dr_flac.h>)
  if(ext == "flac")
  {
    unsigned int ch = 0, sr = 0;
    drflac_uint64 fr = 0;
    if(float* s = drflac_open_file_and_read_pcm_frames_f32(
           path.c_str(), &ch, &sr, &fr, nullptr))
    {
      out.channels = static_cast<int>(ch);
      out.frames = static_cast<std::int64_t>(fr);
      out.rate = static_cast<int>(sr);
      out.interleaved.assign(s, s + fr * ch);
      drflac_free(s, nullptr);
      return out.channels > 0 && out.frames > 0;
    }
  }
#endif
#if __has_include(<dr_mp3.h>)
  if(ext == "mp3")
  {
    drmp3_config cfg{};
    drmp3_uint64 fr = 0;
    if(float* s = drmp3_open_file_and_read_pcm_frames_f32(
           path.c_str(), &cfg, &fr, nullptr))
    {
      out.channels = static_cast<int>(cfg.channels);
      out.frames = static_cast<std::int64_t>(fr);
      out.rate = static_cast<int>(cfg.sampleRate);
      out.interleaved.assign(s, s + fr * cfg.channels);
      drmp3_free(s, nullptr);
      return out.channels > 0 && out.frames > 0;
    }
  }
#endif
#endif // AVND_TD_USE_DRLIBS

  std::vector<unsigned char> buf;
  if(!read_whole_file(path, buf))
    return false;
  return decode_wav(buf.data(), buf.size(), out);
}
} // namespace detail

// One owning slot per file_port.
struct file_slot
{
  std::string current_path;             // last loaded path (change detection)
  std::string contents;                 // owned bytes for text/binary views
  avnd::mmap_file_wrapper mapping;      // owned mapping for mmap views
  std::filesystem::file_time_type mtime{}; // last seen mtime (file_watch)
};

// One owning slot per soundfile_port (float and double variants coexist; only
// the one matching the port's sample type is used).
struct soundfile_slot
{
  std::string current_path;
  std::vector<float> f32;
  std::vector<double> f64;
  std::vector<const float*> ptr_f32;
  std::vector<const double*> ptr_f64;
};

template <typename T>
struct file_ports
{
  static constexpr int file_count = avnd::file_input_introspection<T>::size;
  static constexpr int sndfile_count = avnd::soundfile_input_introspection<T>::size;

  std::array<file_slot, (file_count > 0 ? file_count : 0)> file_slots;
  std::array<soundfile_slot, (sndfile_count > 0 ? sndfile_count : 0)> sndfile_slots;

  void setup(avnd::effect_container<T>& impl, TD::OP_ParameterManager* mgr)
  {
    if constexpr(file_count > 0)
    {
      avnd::file_input_introspection<T>::for_all(
          avnd::get_inputs(impl),
          [&]<typename Field>(Field&) { append_file_param<Field>(mgr); });
    }
    if constexpr(sndfile_count > 0)
    {
      avnd::soundfile_input_introspection<T>::for_all(
          avnd::get_inputs(impl),
          [&]<typename Field>(Field&) { append_file_param<Field>(mgr); });
    }
  }

  void load(avnd::effect_container<T>& impl, const TD::OP_Inputs* inputs)
  {
    if constexpr(file_count > 0)
    {
      avnd::file_input_introspection<T>::for_all_n2(
          avnd::get_inputs(impl),
          [&]<typename Field, std::size_t P, std::size_t I>(
              Field& field, avnd::predicate_index<P>, avnd::field_index<I>) {
        this->template load_file<Field, P>(impl, field, inputs);
      });
    }
    if constexpr(sndfile_count > 0)
    {
      avnd::soundfile_input_introspection<T>::for_all_n2(
          avnd::get_inputs(impl),
          [&]<typename Field, std::size_t P, std::size_t I>(
              Field& field, avnd::predicate_index<P>, avnd::field_index<I>) {
        this->template load_soundfile<Field, P>(impl, field, inputs);
      });
    }
  }

private:
  template <typename Field>
  void append_file_param(TD::OP_ParameterManager* mgr)
  {
    static constexpr auto name = get_td_name<Field>();
    static constexpr auto label = get_parameter_label<Field>();
    TD::OP_StringParameter param(name.data());
    param.label = label.data();
    param.defaultValue = "";
    mgr->appendFile(param);
  }

  template <typename Field, std::size_t P>
  void load_file(avnd::effect_container<T>& impl, Field& field, const TD::OP_Inputs* inputs)
  {
    static constexpr auto name = get_td_name<Field>();
    const char* raw = inputs->getParFilePath(name.data());
    std::string newpath = raw ? raw : "";
    file_slot& slot = file_slots[P];

    if(newpath.empty())
    {
      if(!slot.current_path.empty())
      {
        slot.current_path.clear();
        slot.contents.clear();
        slot.mapping = {};
        field.file.bytes = {};
        field.file.filename = {};
        if_possible(field.update(impl.effect));
      }
      return;
    }

    using file_view_t = std::decay_t<decltype(field.file)>;
    constexpr bool use_mmap = requires { file_view_t::mmap; };

    bool changed = (newpath != slot.current_path);
    if constexpr(avnd::tag_file_watch<Field>)
    {
      if(!changed)
      {
        std::error_code ec;
        auto t = std::filesystem::last_write_time(
            detail::fs_path_from_utf8(newpath), ec);
        if(!ec && t != slot.mtime)
          changed = true;
      }
    }
    if(!changed)
      return;

    slot.current_path = newpath;

    bool ok = false;
    if constexpr(use_mmap)
    {
      try
      {
        slot.mapping = avnd::mmap_file_wrapper(detail::fs_path_from_utf8(newpath));
        auto sp = slot.mapping.as_span();
        field.file.bytes = std::string_view(
            reinterpret_cast<const char*>(sp.data()), sp.size());
        ok = true;
      }
      catch(...)
      {
        slot.mapping = {};
        field.file.bytes = {};
      }
    }
    else
    {
      std::vector<unsigned char> buf;
      if(detail::read_whole_file(newpath, buf))
      {
        slot.contents.assign(reinterpret_cast<const char*>(buf.data()), buf.size());
        field.file.bytes
            = std::string_view(slot.contents.data(), slot.contents.size());
        ok = true;
      }
      else
      {
        slot.contents.clear();
        field.file.bytes = {};
      }
    }

    field.file.filename = std::string_view(slot.current_path);

    std::error_code ec;
    slot.mtime
        = std::filesystem::last_write_time(detail::fs_path_from_utf8(newpath), ec);

    (void)ok;
    if_possible(field.update(impl.effect));
  }

  template <typename Field, std::size_t P>
  void
  load_soundfile(avnd::effect_container<T>& impl, Field& field, const TD::OP_Inputs* inputs)
  {
    static constexpr auto name = get_td_name<Field>();
    const char* raw = inputs->getParFilePath(name.data());
    std::string newpath = raw ? raw : "";
    soundfile_slot& slot = sndfile_slots[P];

    if(newpath == slot.current_path)
      return;
    slot.current_path = newpath;

    auto clear_port = [&] {
      field.soundfile.data = nullptr;
      field.soundfile.frames = 0;
      field.soundfile.channels = 0;
      field.soundfile.filename = {};
    };

    if(newpath.empty())
    {
      clear_port();
      if_possible(field.update(impl.effect));
      return;
    }

    detail::decoded_audio dec;
    if(!detail::decode_audio_file(newpath, dec) || dec.channels <= 0
       || dec.frames <= 0)
    {
      clear_port();
      if_possible(field.update(impl.effect));
      return;
    }

    using channel_ptr_t = avnd::soundfile_channel_type<Field>; // e.g. const float*
    using sample_t = std::remove_const_t<std::remove_pointer_t<channel_ptr_t>>;

    const int ch = dec.channels;
    const std::int64_t fr = dec.frames;

    auto fill = [&](auto& buf, auto& ptrs) {
      buf.resize(static_cast<std::size_t>(ch) * static_cast<std::size_t>(fr));
      for(int c = 0; c < ch; ++c)
        for(std::int64_t f = 0; f < fr; ++f)
          buf[static_cast<std::size_t>(c) * fr + f]
              = static_cast<sample_t>(dec.interleaved[static_cast<std::size_t>(f) * ch + c]);
      ptrs.resize(ch);
      for(int c = 0; c < ch; ++c)
        ptrs[c] = buf.data() + static_cast<std::size_t>(c) * fr;
      field.soundfile.data = ptrs.data();
    };

    if constexpr(std::is_same_v<sample_t, double>)
      fill(slot.f64, slot.ptr_f64);
    else
      fill(slot.f32, slot.ptr_f32);

    field.soundfile.frames = fr;
    field.soundfile.channels = ch;
    if constexpr(requires { field.soundfile.rate; })
      field.soundfile.rate = dec.rate;
    field.soundfile.filename = std::string_view(slot.current_path);

    if_possible(field.update(impl.effect));
  }
};

}
