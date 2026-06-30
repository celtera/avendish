#pragma once

#include <filesystem>
#include <cstddef>
#include <system_error>
#include <span>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace avnd
{
struct mmap_file_wrapper
{
  mmap_file_wrapper() = default;
  explicit mmap_file_wrapper(const std::filesystem::path& path) {
#if defined(_WIN32)
    m_file_handle = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
        );
    if (m_file_handle == INVALID_HANDLE_VALUE) {
      throw std::system_error(GetLastError(), std::system_category(), "CreateFileW failed");
    }

    LARGE_INTEGER file_size{};
    if (!GetFileSizeEx(m_file_handle, &file_size)) {
      CloseHandle(m_file_handle);
      throw std::system_error(GetLastError(), std::system_category(), "GetFileSizeEx failed");
    }
    m_size = static_cast<size_t>(file_size.QuadPart);

    if (m_size == 0) {
      return;
    }

    m_mapping_handle = CreateFileMappingW(
        m_file_handle,
        NULL,
        PAGE_READONLY,
        0,
        0,
        NULL
        );
    if (m_mapping_handle == NULL) {
      CloseHandle(m_file_handle);
      throw std::system_error(GetLastError(), std::system_category(), "CreateFileMappingW failed");
    }

    void* view = MapViewOfFile(
        m_mapping_handle,
        FILE_MAP_READ,
        0,
        0,
        m_size
        );
    if (view == nullptr)
    {
      CloseHandle(m_mapping_handle);
      CloseHandle(m_file_handle);
      throw std::system_error(GetLastError(), std::system_category(), "MapViewOfFile failed");
    }
    m_data = static_cast<std::byte*>(view);

#else
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
      throw std::system_error(errno, std::generic_category(), "open failed");
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
      close(fd); // Cleanup on failure
      throw std::system_error(errno, std::generic_category(), "fstat failed");
    }
    m_size = static_cast<size_t>(sb.st_size);

    // Handle zero-byte files. No need to map.
    if (m_size == 0) {
      close(fd);
      // m_data is already nullptr, m_size is 0.
      return;
    }

    // mmap a file.
    // The mmap persists even after the file descriptor is closed.
    void* mapped_data = mmap(
        NULL,       // Let the kernel choose the address
        m_size,     // Length of the mapping
        PROT_READ,  // Read-only
        MAP_SHARED, // Share changes with other processes
        fd,         // File descriptor
        0           // Offset within the file
        );

    // File descriptor is no longer needed after mmap
    close(fd);

    if (mapped_data == MAP_FAILED) {
      throw std::system_error(errno, std::generic_category(), "mmap failed");
    }
    m_data = static_cast<std::byte*>(mapped_data);
#endif
  }

  ~mmap_file_wrapper() {
    cleanup();
  }

  mmap_file_wrapper(const mmap_file_wrapper&) = delete;
  mmap_file_wrapper& operator=(const mmap_file_wrapper&) = delete;

  mmap_file_wrapper(mmap_file_wrapper&& other) noexcept
      : m_data(other.m_data)
      , m_size(other.m_size)
#if defined(_WIN32)
      , m_file_handle(other.m_file_handle)
      , m_mapping_handle(other.m_mapping_handle)
#endif
  {
    other.m_data = nullptr;
    other.m_size = 0;
#if defined(_WIN32)
    other.m_file_handle = INVALID_HANDLE_VALUE;
    other.m_mapping_handle = NULL;
#endif
  }

  mmap_file_wrapper& operator=(mmap_file_wrapper&& other) noexcept {
    if (this != &other) {
      cleanup();

      m_data = other.m_data;
      m_size = other.m_size;
#if defined(_WIN32)
      m_file_handle = other.m_file_handle;
      m_mapping_handle = other.m_mapping_handle;
#endif

      // 3. Neuter "other"
      other.m_data = nullptr;
      other.m_size = 0;
#if defined(_WIN32)
      other.m_file_handle = INVALID_HANDLE_VALUE;
      other.m_mapping_handle = NULL;
#endif
    }
    return *this;
  }

  [[nodiscard]] const std::byte* data() const noexcept {
    return m_data;
  }

  [[nodiscard]] size_t size() const noexcept {
    return m_size;
  }

  [[nodiscard]] bool empty() const noexcept {
    return m_size == 0;
  }

  [[nodiscard]] std::span<const std::byte> as_span() const noexcept {
    return {m_data, m_size};
  }

private:
  void cleanup() noexcept {
#if defined(_WIN32)
    if (m_data) {
      UnmapViewOfFile(m_data);
      m_data = nullptr;
    }
    if (m_mapping_handle != NULL) {
      CloseHandle(m_mapping_handle);
      m_mapping_handle = NULL;
    }
    if (m_file_handle != INVALID_HANDLE_VALUE) {
      CloseHandle(m_file_handle);
      m_file_handle = INVALID_HANDLE_VALUE;
    }
#else
    if (m_data) {
      munmap(m_data, m_size);
      m_data = nullptr;
    }
#endif
    m_size = 0;
  }

  std::byte* m_data = nullptr;
  size_t m_size = 0;

#if defined(_WIN32)
  HANDLE m_file_handle = INVALID_HANDLE_VALUE;
  HANDLE m_mapping_handle = NULL;
#endif
};
}
