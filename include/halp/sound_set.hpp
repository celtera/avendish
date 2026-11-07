#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later OR BSL-1.0 OR CC0-1.0 OR CC-PDCC OR 0BSD */

#include <halp/file_port.hpp>
#include <halp/static_string.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

namespace halp
{
//! How the host decodes the files of a sound_set.
enum class sound_decoding : uint8_t
{
  all,      //!< Every file, as soon as the folder is listed
  on_demand //!< A file when it is request()ed, then kept
};

enum class sound_status : uint8_t
{
  unloaded, //!< Not decoded yet: request() it, for an on_demand set
  loading,  //!< Being decoded
  ready,    //!< data, frames and channels can be read
  failed    //!< Could not be decoded; it keeps its place in the set
};

//! A file of a sound_set, as the host decoded it, at the engine's rate.
struct sound_view
{
  std::string_view name; //!< Its path, relative to the folder
  const float* const* data{};
  int64_t frames{};
  int32_t channels{};
  int32_t rate{};
  sound_status status{};

  // Owned by the host
  std::atomic<int32_t>* pins{};
  std::atomic<bool>* requested{};
};

/**
 * Keeps a sound of a sound_set alive after the set changes, for as long as the
 * pin exists: e.g. a voice still playing a file that was removed or replaced.
 * Without one, a sound is only valid until the set next changes.
 *
 * Taking, moving and releasing pins never allocates nor frees: the host frees
 * the sounds nobody pins anymore, outside of the audio thread.
 */
class sound_pin
{
public:
  sound_pin() noexcept = default;
  explicit sound_pin(const sound_view& s) noexcept
      : m_pins{s.pins}
  {
    if(m_pins)
      m_pins->fetch_add(1, std::memory_order_relaxed);
  }
  sound_pin(sound_pin&& other) noexcept
      : m_pins{std::exchange(other.m_pins, nullptr)}
  {
  }
  sound_pin& operator=(sound_pin&& other) noexcept
  {
    if(this != &other)
    {
      reset();
      m_pins = std::exchange(other.m_pins, nullptr);
    }
    return *this;
  }
  sound_pin(const sound_pin&) = delete;
  sound_pin& operator=(const sound_pin&) = delete;
  ~sound_pin() { reset(); }

  void reset() noexcept
  {
    if(m_pins)
    {
      m_pins->fetch_sub(1, std::memory_order_release);
      m_pins = nullptr;
    }
  }
  explicit operator bool() const noexcept { return m_pins; }

private:
  std::atomic<int32_t>* m_pins{};
};

//! The files of the folder a sound_set names, as last published by the host.
struct sound_set_view
{
  std::span<const sound_view> sounds;
  std::string_view folder; //!< The folder, resolved
  uint64_t generation{};   //!< Changes each time the set does
};

/**
 * A folder of sound files, listed, decoded and watched by the host.
 *
 * Its value is a path, like a folder_port: a folder names its files, a file
 * names the files of its folder. The files are sorted by their relative path,
 * in natural order; one that cannot be decoded stays in the set as `failed`, so
 * that indices do not move.
 *
 * The set only changes between two ticks. Options, to redefine in a derived
 * struct (functions, as an unnamed struct cannot have static data members):
 *
 *   struct : halp::sound_set<"Sounds"> {
 *     static constexpr std::string_view extensions() { return "wav flac"; }
 *     static constexpr bool recursive() { return true; }
 *     static constexpr auto decoding() { return halp::sound_decoding::on_demand; }
 *   } sounds;
 */
template <static_string lit>
struct sound_set : folder_port<lit>
{
  //! Space-separated file extensions; empty means every audio file the host
  //! can decode.
  static constexpr std::string_view extensions() { return {}; }
  static constexpr bool recursive() { return false; }
  static constexpr sound_decoding decoding() { return sound_decoding::all; }

  [[nodiscard]] std::size_t size() const noexcept { return files.sounds.size(); }
  [[nodiscard]] bool empty() const noexcept { return files.sounds.empty(); }
  const sound_view& operator[](std::size_t i) const noexcept { return files.sounds[i]; }
  [[nodiscard]] uint64_t generation() const noexcept { return files.generation; }

  //! The index of the file with that relative path, or -1
  [[nodiscard]] int find(std::string_view name) const noexcept
  {
    for(std::size_t i = 0; i < files.sounds.size(); i++)
      if(files.sounds[i].name == name)
        return int(i);
    return -1;
  }

  //! On a set decoded on_demand: asks for a file to be decoded. It becomes
  //! ready in a later set. Never allocates.
  void request(std::size_t i) const noexcept
  {
    const auto& s = files.sounds[i];
    if(s.requested && s.status == sound_status::unloaded)
      s.requested->store(true, std::memory_order_relaxed);
  }

  //! Keeps a file alive after the set changes, see sound_pin
  [[nodiscard]] sound_pin pin(std::size_t i) const noexcept
  {
    return sound_pin{files.sounds[i]};
  }

  sound_set_view files;
};
}
