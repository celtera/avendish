#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/dataflow/value_port.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace examples
{
enum class FileIOMode
{
  Async,
  Sync
};
enum class FileReadMode
{
  Whole,
  Range,
  Stream,
  Autostream
};
enum class FileWriteMode
{
  Ignore,
  Write,
  WriteRange,
  Append
};
enum class FileLineEnding : std::uint8_t
{
  None,
  LF,
  CRLF
};

namespace file_io
{
inline constexpr int max_file_bytes = 64 * 1024 * 1024;
inline constexpr int max_line_bytes = 1024 * 1024;

enum class Operation : std::uint8_t
{
  Read,
  ReadRange,
  ReadStream,
  Autostream,
  Open,
  Next,
  Rewind,
  Close,
  Write,
  Append,
  WriteRange
};

// Jobs and completions own only values, never an object pointer or an open file.
// A line session stores a path and byte offset, reopening and seeking for each
// requested line. This streams without preloading, and cannot close a handle on
// the processor thread at destruction. Do not edit/replace a file mid-session.
struct Request
{
  Operation operation{};
  FileLineEnding ending{};
  int timeout_ms{1000};
  std::string path;
  std::string data;
  std::size_t limit{};
  std::uint64_t offset{};
  std::uint64_t generation{};
};

struct Result
{
  Operation operation{};
  std::string path;
  std::string data;
  std::string error;
  std::uint64_t offset{};
  std::int64_t bytes{};
  std::int64_t lines{};
  bool has_line{};
  bool eof{};
  bool timed_out{};
  bool publish_partial{};
  std::uint64_t generation{};
};

inline std::int64_t count_lines(std::string_view text)
{
  if(text.empty())
    return 0;
  return std::count(text.begin(), text.end(), '\n') + (text.back() != '\n');
}

// Stream pulls are independent open/read/close requests, not persistent sessions.
// Only the worker owns descriptors. Never transfer one to a completion callback.
#if defined(__unix__) || defined(__APPLE__)
inline void pull_stream(int fd, const Request& request, Result& result)
{
  const auto deadline
      = std::chrono::steady_clock::now() + std::chrono::milliseconds{request.timeout_ms};
  std::array<char, 8192> buffer;
  while(result.data.size() < request.limit)
  {
    const auto now = std::chrono::steady_clock::now();
    if(now >= deadline)
    {
      result.timed_out = true;
      result.error = "Stream pull timed out; Data contains bytes received";
      break;
    }
    const auto received = ::read(
        fd, buffer.data(), std::min(buffer.size(), request.limit - result.data.size()));
    if(received > 0)
    {
      result.data.append(buffer.data(), static_cast<std::size_t>(received));
      continue;
    }
    if(received == 0)
    {
      result.eof = true;
      break;
    }
    if(errno == EINTR)
      continue;
    if(errno != EAGAIN && errno != EWOULDBLOCK)
    {
      result.error
          = "Stream read: " + std::error_code{errno, std::generic_category()}.message();
      break;
    }
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
                               deadline - std::chrono::steady_clock::now())
                               .count();
    if(remaining <= 0)
      continue;
    pollfd descriptor{fd, POLLIN, 0};
    const auto polled = ::poll(&descriptor, 1, static_cast<int>(remaining));
    if(polled < 0 && errno != EINTR)
    {
      result.error
          = "Stream poll: " + std::error_code{errno, std::generic_category()}.message();
      break;
    }
    if(polled > 0 && (descriptor.revents & POLLNVAL))
    {
      result.error = "Invalid stream descriptor";
      break;
    }
  }
  result.bytes = static_cast<std::int64_t>(result.data.size());
  result.lines = count_lines(result.data);
  // On timeout or I/O error consumers still receive the bytes already consumed.
  result.publish_partial = true;
}
#endif
// Called exclusively by work(), or explicitly on the caller in Sync mode.
// Success for writes means stream close succeeded, not fsync / crash durability.
inline Result perform(Request request)
{
  Result result;
  result.operation = request.operation;
  result.generation = request.generation;
  try
  {
    if(request.operation == Operation::Close)
      return result;
    const bool bounded = request.operation == Operation::ReadRange
                         || request.operation == Operation::ReadStream
                         || request.operation == Operation::Autostream
                         || request.operation == Operation::WriteRange;
    if(bounded && request.limit == 0)
      return result; // No open, seek, read or write; EOF remains unknown (false).
    if(bounded && request.limit > max_file_bytes)
    {
      result.error = "Count is out of range";
      return result;
    }
    if(request.operation == Operation::ReadStream
       || request.operation == Operation::Autostream)
    {
#if defined(__unix__) || defined(__APPLE__)
      if(request.timeout_ms < 1 || request.timeout_ms > 60000)
      {
        result.error = "Stream timeout is out of range";
        return result;
      }
      struct Descriptor
      {
        int fd;
        ~Descriptor()
        {
          if(fd >= 0)
            ::close(fd);
        }
      } descriptor{
          ::open(request.path.c_str(), O_RDONLY | O_NONBLOCK | O_NOCTTY | O_CLOEXEC)};
      if(descriptor.fd < 0)
      {
        result.error = "Stream open: "
                       + std::error_code{errno, std::generic_category()}.message();
        return result;
      }
      struct stat status{};
      if(::fstat(descriptor.fd, &status) != 0)
      {
        result.error = "Stream status: "
                       + std::error_code{errno, std::generic_category()}.message();
        return result;
      }
      if((!S_ISREG(status.st_mode) && !S_ISCHR(status.st_mode))
         || ::isatty(descriptor.fd))
      {
        result.error
            = "Stream pulls reopen each request; FIFOs, sockets, terminals "
              "and other session sources are unsupported";
        return result;
      }
      if(request.operation == Operation::Autostream && S_ISREG(status.st_mode))
      {
        if(request.offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())
           || ::lseek(descriptor.fd, static_cast<off_t>(request.offset), SEEK_SET) < 0)
        {
          result.error = "Stream offset is out of range or seek failed";
          return result;
        }
      }
      pull_stream(descriptor.fd, request, result);
      if(request.operation == Operation::Autostream)
      {
        result.offset = request.offset + result.data.size();
        if(S_ISREG(status.st_mode) && status.st_size >= 0)
          result.eof |= result.offset >= static_cast<std::uint64_t>(status.st_size);
      }
      return result;
#else
      if(request.operation == Operation::ReadStream)
      {
        result.error = "Non-seeking stream pulls require POSIX nonblocking I/O";
        return result;
      }
      request.operation = Operation::ReadRange;
#endif
    }

    const std::filesystem::path path{std::u8string_view{
        reinterpret_cast<const char8_t*>(request.path.data()), request.path.size()}};
    std::error_code ec;
    const auto status = std::filesystem::status(path, ec);
    const bool writing = request.operation == Operation::Write
                         || request.operation == Operation::Append
                         || request.operation == Operation::WriteRange;
    const bool missing = status.type() == std::filesystem::file_type::not_found;
    const bool may_create = writing && request.operation != Operation::WriteRange;
    if((ec && !(may_create && missing))
       || (!std::filesystem::is_regular_file(status) && !(may_create && missing)))
    {
      result.error = ec ? "File status: " + ec.message() : "Not a regular file";
      return result;
    }

    if(request.offset
       > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
    {
      result.error = "File offset is out of range";
      return result;
    }
    if(writing)
    {
      if(request.ending == FileLineEnding::LF)
        request.data += '\n';
      else if(request.ending == FileLineEnding::CRLF)
        request.data += "\r\n";
      if(request.operation == Operation::WriteRange)
      {
        const auto count = std::min(request.limit, request.data.size());
        if(count > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())
                       - request.offset)
        {
          result.error = "File range is out of range";
          return result;
        }
        std::fstream stream{path, std::ios::binary | std::ios::in | std::ios::out};
        if(!stream)
        {
          result.error = "Cannot open existing file for range writing";
          return result;
        }
        stream.seekp(static_cast<std::streamoff>(request.offset));
        if(!stream)
        {
          result.error = "File seek failed";
          return result;
        }
        stream.write(request.data.data(), static_cast<std::streamsize>(count));
        stream.close();
        if(!stream)
          result.error = "Write or close failed; file may be partially written";
        else
        {
          result.bytes = static_cast<std::int64_t>(count);
          result.lines = count_lines(std::string_view{request.data}.substr(0, count));
        }
        return result;
      }
      std::ofstream stream{
          path, std::ios::binary | std::ios::out
                    | (request.operation == Operation::Append ? std::ios::app
                                                              : std::ios::trunc)};
      if(!stream)
      {
        result.error = "Cannot open file for writing";
        return result;
      }
      stream.write(
          request.data.data(), static_cast<std::streamsize>(request.data.size()));
      stream.close();
      if(!stream)
        result.error = "Write or close failed; file may be partially written";
      else
      {
        result.bytes = static_cast<std::int64_t>(request.data.size());
        result.lines = count_lines(request.data);
      }
      return result;
    }

    std::ifstream stream{path, std::ios::binary};
    if(!stream)
    {
      result.error = "Cannot open file for reading";
      return result;
    }
    if(request.operation == Operation::ReadRange)
    {
      stream.seekg(0, std::ios::end);
      const auto end = stream.tellg();
      if(end < 0)
      {
        result.error = "Cannot determine file size";
        return result;
      }
      const auto size = static_cast<std::uint64_t>(end);
      if(request.offset >= size)
      {
        result.eof = true;
        return result;
      }
      stream.seekg(static_cast<std::streamoff>(request.offset));
      if(!stream)
      {
        result.error = "File seek failed";
        return result;
      }
      // Never probe an extra byte: an exact boundary is known from the size.
      const auto count = std::min<std::uint64_t>(request.limit, size - request.offset);
      result.data.resize(static_cast<std::size_t>(count));
      stream.read(result.data.data(), static_cast<std::streamsize>(count));
      result.data.resize(static_cast<std::size_t>(stream.gcount()));
      result.bytes = static_cast<std::int64_t>(result.data.size());
      result.lines = count_lines(result.data);
      result.eof = stream.eof() || result.data.size() >= size - request.offset;
      result.offset = request.offset + result.data.size();
      if(stream.bad() || (stream.fail() && !stream.eof()))
        result.error = "File range read failed";
      return result;
    }
    if(request.operation == Operation::Read)
    {
      std::array<char, 8192> buffer;
      while(stream)
      {
        // One extra byte distinguishes an exact-limit file from truncation.
        const auto count
            = std::min(buffer.size(), request.limit - result.data.size() + 1);
        stream.read(buffer.data(), static_cast<std::streamsize>(count));
        const auto received = static_cast<std::size_t>(stream.gcount());
        if(received > request.limit - result.data.size())
        {
          result.error = "File exceeds maximum bytes";
          break;
        }
        result.data.append(buffer.data(), received);
      }
      if(result.error.empty() && (stream.bad() || !stream.eof()))
        result.error = "File read failed";
      if(result.error.empty())
      {
        result.bytes = static_cast<std::int64_t>(result.data.size());
        result.lines = count_lines(result.data);
        result.eof = true;
      }
      return result;
    }

    if(request.operation == Operation::Open || request.operation == Operation::Rewind)
    {
      result.path = std::move(request.path);
      result.eof = stream.peek() == std::char_traits<char>::eof();
      if(stream.bad())
        result.error = "File read failed";
      return result;
    }

    stream.seekg(static_cast<std::streamoff>(request.offset));
    if(!stream)
    {
      result.error = "File seek failed";
      return result;
    }
    result.offset = request.offset;
    for(;;)
    {
      const auto character = stream.get();
      if(character == std::char_traits<char>::eof())
      {
        result.eof = true;
        if(stream.bad())
          result.error = "File read failed";
        break;
      }
      ++result.offset;
      result.has_line = true;
      if(character == '\n')
      {
        if(!result.data.empty() && result.data.back() == '\r')
          result.data.pop_back();
        result.eof = stream.peek() == std::char_traits<char>::eof();
        if(stream.bad())
          result.error = "File read failed";
        break;
      }
      result.data += static_cast<char>(character);
      // Permit one trailing CR beyond the payload limit, only for CRLF.
      if(result.data.size() > request.limit
         && !(result.data.size() == request.limit + 1 && character == '\r'))
      {
        result.error = "Line exceeds maximum bytes; cursor unchanged";
        break;
      }
    }
    if(result.data.size() > request.limit && result.error.empty())
      result.error = "Line exceeds maximum bytes; cursor unchanged";
    result.bytes = static_cast<std::int64_t>(result.offset - request.offset);
    result.lines = result.has_line ? 1 : 0;
    return result;
  }
  catch(const std::exception& error)
  {
    result.error = error.what();
    return result;
  }
}

template <typename Object>
struct Worker
{
  std::function<void(Request)> request;
  std::optional<Result> completed;

  static std::function<void(Object&)> work(Request request)
  {
    auto result = perform(std::move(request));
    return [result = std::move(result)](Object& self) mutable {
      // Execution commands arrive before graph ports are cleared. Stage here;
      // callbacks must be emitted by operator() during the next processing tick.
      self.worker.completed.emplace(std::move(result));
    };
  }

  void publish(Object& self)
  {
    if(!completed)
      return;
    auto result = std::move(*completed);
    completed.reset();
    if constexpr(requires { self.consume_stream(result); })
      if(self.consume_stream(result))
        return;
    // Keep Busy during Data/Line callbacks; Success is the pacing signal.
    if(result.error.empty() || result.publish_partial)
      self.apply(result);
    self.outputs.busy = false;
    if(result.error.empty())
      self.outputs.success();
    else
      self.outputs.error(std::move(result.error));
  }
};

template <typename Object>
bool ready(Object& self)
{
  if(self.outputs.busy.value)
    self.outputs.error("Busy: request rejected");
  else if(self.inputs.mode.value == FileIOMode::Async && !self.worker.request)
    self.outputs.error("Async worker unavailable: request rejected");
  else if(
      self.inputs.mode.value != FileIOMode::Async
      && self.inputs.mode.value != FileIOMode::Sync)
    self.outputs.error("Invalid I/O mode");
  else
    return true;
  return false;
}

template <typename Object>
bool valid_path(Object& self, const std::string& path)
{
  if(path.empty() || path.size() > 32768 || path.find('\0') != std::string::npos)
  {
    self.outputs.error("Invalid file path");
    return false;
  }
  return true;
}

template <typename Object>
bool valid_limit(Object& self, int limit, int maximum)
{
  if(limit < 1 || limit > maximum)
  {
    self.outputs.error("Byte limit is out of range");
    return false;
  }
  return true;
}

template <typename Object>
bool valid_offset(Object& self, std::uint64_t& offset)
{
  // Decimal text is lossless through hosts whose numeric ports are only int32.
  const auto& text = self.inputs.offset.value;
  std::int64_t parsed{};
  const auto [end, error]
      = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if(error != std::errc{} || end != text.data() + text.size() || parsed < 0)
  {
    self.outputs.error("Offset must be a non-negative signed 64-bit decimal integer");
    return false;
  }
  offset = static_cast<std::uint64_t>(parsed);
  return true;
}

template <typename Object>
bool valid_count(Object& self)
{
  if(self.inputs.count.value < 0 || self.inputs.count.value > max_file_bytes)
  {
    self.outputs.error("Count is out of range");
    return false;
  }
  return true;
}

template <typename Object>
void submit(Object& self, Request request)
{
  self.outputs.busy = true;
  if(self.inputs.mode.value == FileIOMode::Sync)
  {
    Worker<Object>::work(std::move(request))(self);
    self.worker.publish(self);
  }
  else
  {
    try
    {
      self.worker.request(std::move(request));
    }
    catch(const std::exception& error)
    {
      self.outputs.busy = false;
      self.outputs.error(error.what());
    }
  }
}
}

struct FileRead
{
  halp_meta(name, "Read File")
  halp_meta(c_name, "avnd_read_file")
  halp_meta(category, "Control/Files")
  halp_meta(author, "ossia score")
  halp_meta(
      description,
      "Read Whole, Range or Stream on demand, or Autostream chunks each tick. "
      "Async Autostream prefetches bounded batches; startup and slow sources may "
      "starve. Regular files advance; devices reopen on the worker. No FIFO sessions.")
  halp_meta(uuid, "f82d1b69-c381-4eef-86c3-506d42b3d8e1")

  void read()
  {
    if(inputs.read_mode.value == FileReadMode::Autostream)
      return; // Autostream is paced exclusively by processing ticks.
    if(!file_io::ready(*this) || !file_io::valid_path(*this, inputs.path.value))
      return;
    file_io::Request request{
        .operation = file_io::Operation::Read, .path = inputs.path.value};
    switch(inputs.read_mode.value)
    {
      case FileReadMode::Whole:
        if(!file_io::valid_limit(*this, inputs.max_bytes.value, file_io::max_file_bytes))
          return;
        request.limit = static_cast<std::size_t>(inputs.max_bytes.value);
        break;
      case FileReadMode::Range:
      case FileReadMode::Stream:
        if(!file_io::valid_count(*this) || !file_io::valid_offset(*this, request.offset))
          return;
        request.limit = static_cast<std::size_t>(inputs.count.value);
        if(inputs.read_mode.value == FileReadMode::Range)
          request.operation = file_io::Operation::ReadRange;
        else
        {
          if(request.offset != 0)
          {
            outputs.error("Stream pulls do not seek; Offset must be zero");
            return;
          }
          if(inputs.timeout_ms.value < 1 || inputs.timeout_ms.value > 60000)
          {
            outputs.error("Stream timeout is out of range");
            return;
          }
          request.operation = file_io::Operation::ReadStream;
          request.timeout_ms = inputs.timeout_ms.value;
        }
        break;
      default:
        outputs.error("Invalid read mode");
        return;
    }
    file_io::submit(*this, std::move(request));
  }

  struct
  {
    halp::lineedit<"Path", ""> path;
    halp::enum_t<FileIOMode, "Execution"> mode;
    halp::spinbox_i32<"Maximum bytes", halp::range{1, file_io::max_file_bytes, 1048576}>
        max_bytes;
    halp::impulse_button<"Read"> read;
    halp::enum_t<FileReadMode, "Mode"> read_mode;
    struct : halp::lineedit<"Offset", "0">
    {
      using halp::lineedit<"Offset", "0">::operator=;
      halp_meta(
          description,
          "Non-negative signed 64-bit decimal byte offset for Range. Autostream starts "
          "at zero.")
    } offset;
    struct : halp::spinbox_i32<"Count", halp::range{0, file_io::max_file_bytes, 65536}>
    {
      halp_meta(
          description,
          "Bytes per Range or Stream request, or per Autostream tick. Zero performs no "
          "I/O. A terminal chunk may be shorter.")
    } count;
    struct : halp::spinbox_i32<"Timeout", halp::range{1, 60000, 1000}>
    {
      halp_meta(
          description,
          "Stream worker deadline in milliseconds. Timeouts report Error; Autostream "
          "retains partial bytes until a chunk is ready.")
    } timeout_ms;
  } inputs;

  struct
  {
    halp::callback<"Data", std::string> data;
    halp::val_port<"Bytes", std::int64_t> bytes;
    halp::val_port<"Lines", std::int64_t> lines;
    halp::val_port<"EOF", bool> eof;
    halp::val_port<"Busy", bool> busy;
    halp::callback<"Success"> success;
    halp::callback<"Error", std::string> error;
    halp::val_port<"Timed out", bool> timed_out;
  } outputs;

  file_io::Worker<FileRead> worker;
  struct StreamSettings
  {
    std::string path;
    FileReadMode read_mode{};
    FileIOMode execution{};
    int count{};
    int timeout{};
  } stream_settings;
  std::string stream_buffer;
  std::size_t stream_consumed{};
  std::uint64_t stream_offset{};
  bool stream_eof{};
  std::uint64_t stream_generation{};
  bool stream_finished{};
  bool stream_enabled{true};

  void reset_stream()
  {
    ++stream_generation;
    stream_buffer.clear();
    stream_consumed = 0;
    stream_offset = 0;
    stream_eof = false;
    stream_finished = false;
    outputs.eof = false;
    outputs.timed_out = false;
    // Do not clear Busy: an invalidated worker must finish before another starts.
  }

  void start()
  {
    stream_enabled = true;
    reset_stream();
  }

  void stop()
  {
    stream_enabled = false;
    reset_stream();
  }

  void update_stream_settings()
  {
    if(stream_settings.path != inputs.path.value
       || stream_settings.read_mode != inputs.read_mode.value
       || stream_settings.execution != inputs.mode.value
       || stream_settings.count != inputs.count.value
       || stream_settings.timeout != inputs.timeout_ms.value)
    {
      stream_settings
          = {inputs.path.value, inputs.read_mode.value, inputs.mode.value,
             inputs.count.value, inputs.timeout_ms.value};
      reset_stream();
    }
  }

  bool consume_stream(file_io::Result& result)
  {
    if(result.operation != file_io::Operation::Autostream)
    {
      if(inputs.read_mode.value != FileReadMode::Autostream)
        return false;
      // An on-demand request accepted before entering Autostream is not a chunk.
      outputs.busy = false;
      return true;
    }
    outputs.busy = false;
    if(result.generation != stream_generation || !stream_enabled
       || inputs.read_mode.value != FileReadMode::Autostream)
      return true;
    stream_offset = result.offset;
    if(stream_buffer.empty())
      stream_buffer = std::move(result.data);
    else
      stream_buffer += result.data;
    stream_finished = result.eof || (!result.error.empty() && !result.timed_out);
    outputs.timed_out = result.timed_out;
    stream_eof = result.eof;
    if(!result.error.empty())
      outputs.error(std::move(result.error));
    return true;
  }

  void autostream()
  {
    if(!stream_enabled)
      return;
    if(!stream_finished && !outputs.busy.value)
    {
      if(!file_io::valid_count(*this) || !file_io::valid_path(*this, inputs.path.value)
         || inputs.timeout_ms.value < 1 || inputs.timeout_ms.value > 60000)
      {
        if(inputs.timeout_ms.value < 1 || inputs.timeout_ms.value > 60000)
          outputs.error("Stream timeout is out of range");
        stream_finished = true;
        return;
      }
      const auto count = static_cast<std::size_t>(inputs.count.value);
      if(count == 0)
        return;
      const auto batch
          = inputs.mode.value == FileIOMode::Sync
                ? count
                : std::min<std::size_t>(count * 32, file_io::max_file_bytes);
      // Refill early while buffered chunks cover worker latency. At most one
      // request and one bounded buffer exist; no locks or I/O on Async ticks.
      if(stream_buffer.size() - stream_consumed <= batch / 2)
      {
        if(!file_io::ready(*this))
        {
          stream_finished = true;
          return;
        }
        stream_buffer.erase(0, stream_consumed);
        stream_consumed = 0;
        file_io::Request request{
            .operation = file_io::Operation::Autostream, .path = inputs.path.value};
        request.limit = batch;
        request.offset = stream_offset;
        request.timeout_ms = inputs.timeout_ms.value;
        request.generation = stream_generation;
        file_io::submit(*this, std::move(request));
      }
    }
    const auto available = stream_buffer.size() - stream_consumed;
    const auto count = static_cast<std::size_t>(std::max(0, inputs.count.value));
    if(count && (available >= count || (stream_finished && available)))
    {
      const auto size = std::min(count, available);
      auto data = stream_buffer.substr(stream_consumed, size);
      stream_consumed += size;
      outputs.bytes = static_cast<std::int64_t>(size);
      outputs.lines = file_io::count_lines(data);
      outputs.eof = stream_eof && stream_consumed == stream_buffer.size();
      outputs.data(std::move(data));
      outputs.success();
    }
    else if(stream_finished && !available)
      outputs.eof = stream_eof;
  }
  void operator()()
  {
    update_stream_settings();
    if(inputs.read_mode.value == FileReadMode::Autostream)
    {
      worker.publish(*this);
      autostream();
    }
    else
    {
      if(inputs.read.value)
        read();
      worker.publish(*this);
    }
  }

  void apply(file_io::Result& result)
  {
    outputs.bytes = result.bytes;
    outputs.lines = result.lines;
    outputs.eof = result.eof;
    outputs.timed_out = result.timed_out;
    outputs.data(std::move(result.data));
  }
};

struct FileReadLine
{
  halp_meta(name, "Read File Line")
  halp_meta(c_name, "avnd_read_file_line")
  halp_meta(category, "Control/Files")
  halp_meta(author, "ossia score")
  halp_meta(
      description,
      "Stream one bounded LF/CRLF line per Next; preserve blank lines. Async by "
      "default.")
  halp_meta(uuid, "0d2bd0c7-392c-45ec-bd70-6be4937f0348")

  // Processor-owned cursor; no shared worker state or mutexes.
  std::string opened_path;
  std::uint64_t offset{};

  void open()
  {
    if(file_io::ready(*this) && file_io::valid_path(*this, inputs.path.value))
      file_io::submit(
          *this, {.operation = file_io::Operation::Open, .path = inputs.path.value});
  }
  void next()
  {
    if(!file_io::ready(*this))
      return;
    if(!outputs.open.value)
    {
      outputs.error("No file is open");
      return;
    }
    if(file_io::valid_limit(*this, inputs.max_bytes.value, file_io::max_line_bytes))
      file_io::submit(
          *this, {.operation = file_io::Operation::Next,
                  .path = opened_path,
                  .limit = static_cast<std::size_t>(inputs.max_bytes.value),
                  .offset = offset});
  }
  void rewind()
  {
    if(!file_io::ready(*this))
      return;
    if(!outputs.open.value)
      outputs.error("No file is open");
    else
      file_io::submit(
          *this, {.operation = file_io::Operation::Rewind, .path = opened_path});
  }
  void close()
  {
    if(file_io::ready(*this))
      file_io::submit(*this, {.operation = file_io::Operation::Close});
  }

  struct
  {
    halp::lineedit<"Path", ""> path;
    halp::enum_t<FileIOMode, "Mode"> mode;
    halp::spinbox_i32<
        "Maximum line bytes", halp::range{1, file_io::max_line_bytes, 65536}>
        max_bytes;
    halp::impulse_button<"Open"> open;
    halp::impulse_button<"Next"> next;
    halp::impulse_button<"Rewind"> rewind;
    halp::impulse_button<"Close"> close;
  } inputs;

  struct
  {
    halp::callback<"Line", std::string> line;
    halp::val_port<"Line number", std::int64_t> lines;
    halp::val_port<"Bytes", std::int64_t> bytes;
    halp::val_port<"Open", bool> open;
    halp::val_port<"EOF", bool> eof;
    halp::val_port<"Busy", bool> busy;
    halp::callback<"Success"> success;
    halp::callback<"Error", std::string> error;
  } outputs;

  file_io::Worker<FileReadLine> worker;
  void operator()()
  {
    if(inputs.open.value)
      open();
    if(inputs.next.value)
      next();
    if(inputs.rewind.value)
      rewind();
    if(inputs.close.value)
      close();
    worker.publish(*this);
  }

  void apply(file_io::Result& result)
  {
    outputs.bytes = result.bytes;
    outputs.eof = result.eof;
    offset = result.offset;
    if(result.operation == file_io::Operation::Close)
    {
      opened_path.clear();
      outputs.open = false;
      outputs.lines = 0;
    }
    else if(
        result.operation == file_io::Operation::Open
        || result.operation == file_io::Operation::Rewind)
    {
      opened_path = std::move(result.path);
      outputs.open = true;
      outputs.lines = 0;
    }
    else if(result.has_line)
    {
      ++outputs.lines.value;
      outputs.line(std::move(result.data));
    }
  }
};

struct FileWrite
{
  halp_meta(name, "Write File")
  halp_meta(c_name, "avnd_write_file")
  halp_meta(category, "Control/Files")
  halp_meta(author, "ossia score")
  halp_meta(
      description,
      "Each Data event performs Mode: Write replaces the entire file, WriteRange "
      "overwrites at Offset without truncation, Append adds continuous chunks, "
      "Ignore does nothing. Async rejects busy events explicitly; no write queue.")
  halp_meta(uuid, "810761ce-52ea-4105-8bb3-598d5692c20f")

  void receive(const std::string& data)
  {
    file_io::Operation operation;
    switch(inputs.action.value)
    {
      case FileWriteMode::Ignore:
        return;
      case FileWriteMode::Write:
        operation = file_io::Operation::Write;
        break;
      case FileWriteMode::WriteRange:
        operation = file_io::Operation::WriteRange;
        break;
      case FileWriteMode::Append:
        operation = file_io::Operation::Append;
        break;
      default:
        outputs.error("Invalid write mode");
        return;
    }
    if(!file_io::ready(*this) || !file_io::valid_path(*this, inputs.path.value)
       || !file_io::valid_limit(*this, inputs.max_bytes.value, file_io::max_file_bytes))
      return;
    if(inputs.ending.value != FileLineEnding::None
       && inputs.ending.value != FileLineEnding::LF
       && inputs.ending.value != FileLineEnding::CRLF)
    {
      outputs.error("Invalid line ending");
      return;
    }
    const std::size_t suffix = inputs.ending.value == FileLineEnding::CRLF ? 2
                               : inputs.ending.value == FileLineEnding::LF ? 1
                                                                           : 0;
    if(data.size() + suffix > static_cast<std::size_t>(inputs.max_bytes.value))
    {
      outputs.error("Data exceeds maximum bytes; file unchanged");
      return;
    }
    std::uint64_t offset{};
    if(operation == file_io::Operation::WriteRange
       && (!file_io::valid_count(*this) || !file_io::valid_offset(*this, offset)))
      return;
    file_io::submit(
        *this, {.operation = operation,
                .ending = inputs.ending.value,
                .path = inputs.path.value,
                .data = operation == file_io::Operation::WriteRange
                            ? data.substr(0, static_cast<std::size_t>(inputs.count.value))
                            : data,
                .limit = operation == file_io::Operation::WriteRange
                             ? static_cast<std::size_t>(inputs.count.value)
                             : static_cast<std::size_t>(inputs.max_bytes.value),
                .offset = offset});
  }

  struct
  {
    struct
    {
      static constexpr bool is_event() { return true; }
      halp_meta(name, "Data")
      halp_meta(
          description,
          "Binary string events. Every arrival, including equal values at the same "
          "timestamp, triggers Mode. No retained value is replayed.")
      ossia::value_port* value{};
    } data;
    halp::lineedit<"Path", ""> path;
    struct : halp::enum_t<FileWriteMode, "Mode">
    {
      using halp::enum_t<FileWriteMode, "Mode">::operator=;
      halp_meta(
          description,
          "Write replaces the file on each event. WriteRange preserves surrounding "
          "bytes. Append adds chunks. Ignore performs no I/O.")
    } action;
    halp::enum_t<FileIOMode, "Execution"> mode;
    halp::enum_t<FileLineEnding, "Line ending"> ending;
    halp::spinbox_i32<"Maximum bytes", halp::range{1, file_io::max_file_bytes, 1048576}>
        max_bytes;
    struct : halp::lineedit<"Offset", "0">
    {
      using halp::lineedit<"Offset", "0">::operator=;
      halp_meta(
          description, "Non-negative signed 64-bit decimal byte offset for WriteRange.")
    } offset;
    struct : halp::spinbox_i32<"Count", halp::range{0, file_io::max_file_bytes, 65536}>
    {
      halp_meta(
          description,
          "Maximum bytes written by WriteRange, including the selected line ending. "
          "Zero performs no I/O.")
    } count;
  } inputs;

  struct
  {
    halp::val_port<"Bytes", std::int64_t> bytes;
    halp::val_port<"Lines", std::int64_t> lines;
    halp::val_port<"Busy", bool> busy;
    halp::callback<"Success"> success;
    halp::callback<"Error", std::string> error;
  } outputs;

  file_io::Worker<FileWrite> worker;
  void operator()()
  {
    if(inputs.data.value && inputs.action.value != FileWriteMode::Ignore)
    {
      for(const auto& event : inputs.data.value->get_data())
      {
        if(const auto* data = event.value.target<std::string>())
          receive(*data);
        else
          outputs.error("Data must be a binary string");
      }
    }
    worker.publish(*this);
  }

  void apply(file_io::Result& result)
  {
    outputs.bytes = result.bytes;
    outputs.lines = result.lines;
  }
};
}
