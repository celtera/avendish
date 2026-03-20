#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <system_error>
// Platform-specific includes
#if defined(_WIN32)
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

using socket_t = SOCKET;
constexpr socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
using socket_t = int;
constexpr socket_t INVALID_SOCKET_VALUE = -1;
#endif

namespace netstream
{

// Cross-platform socket error handling
inline int get_socket_error()
{
#if defined(_WIN32)
  return WSAGetLastError();
#else
  return errno;
#endif
}

inline void set_non_blocking(socket_t sock)
{
#if defined(_WIN32)
  u_long mode = 1;
  ioctlsocket(sock, FIONBIO, &mode);
#else
  int flags = fcntl(sock, F_GETFL, 0);
  fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

inline void set_blocking(socket_t sock)
{
#if defined(_WIN32)
  u_long mode = 0;
  ioctlsocket(sock, FIONBIO, &mode);
#else
  int flags = fcntl(sock, F_GETFL, 0);
  fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif
}

// Unblock any threads blocked in recv()/send() on this socket.
// Unlike close(), shutdown() is guaranteed to wake them up.
inline void shutdown_socket(socket_t sock)
{
  if(sock != INVALID_SOCKET_VALUE)
  {
#if defined(_WIN32)
    shutdown(sock, SD_BOTH);
#else
    shutdown(sock, SHUT_RDWR);
#endif
  }
}

inline void close_socket(socket_t sock)
{
  if(sock != INVALID_SOCKET_VALUE)
  {
#if defined(_WIN32)
    closesocket(sock);
#else
    close(sock);
#endif
  }
}

// RAII socket wrapper
class Socket
{
public:
  Socket() = default;

  explicit Socket(socket_t sock)
      : m_socket(sock)
  {
  }

  ~Socket() { close(); }

  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  Socket(Socket&& other) noexcept
      : m_socket(other.m_socket)
  {
    other.m_socket = INVALID_SOCKET_VALUE;
  }

  Socket& operator=(Socket&& other) noexcept
  {
    if(this != &other)
    {
      close();
      m_socket = other.m_socket;
      other.m_socket = INVALID_SOCKET_VALUE;
    }
    return *this;
  }

  bool is_valid() const noexcept { return m_socket != INVALID_SOCKET_VALUE; }

  socket_t native_handle() const noexcept { return m_socket; }

  // Unblock threads blocked in recv/send, but keep the fd valid
  // so close() can still clean up after thread join.
  void shutdown()
  {
    if(is_valid())
      shutdown_socket(m_socket);
  }

  void close()
  {
    if(is_valid())
    {
      close_socket(m_socket);
      m_socket = INVALID_SOCKET_VALUE;
    }
  }

  socket_t release() noexcept
  {
    socket_t sock = m_socket;
    m_socket = INVALID_SOCKET_VALUE;
    return sock;
  }

private:
  socket_t m_socket = INVALID_SOCKET_VALUE;
};

// Initialize Winsock on Windows
class SocketInitializer
{
public:
  SocketInitializer()
  {
#if defined(_WIN32)
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
      throw std::runtime_error("WSAStartup failed");
    }
#endif
  }

  ~SocketInitializer()
  {
#if defined(_WIN32)
    WSACleanup();
#endif
  }

  SocketInitializer(const SocketInitializer&) = delete;
  SocketInitializer& operator=(const SocketInitializer&) = delete;
};

// Get the global socket initializer
inline SocketInitializer& get_socket_initializer()
{
  static SocketInitializer init;
  return init;
}

// Create a TCP socket
inline Socket create_tcp_socket()
{
  get_socket_initializer(); // Ensure Winsock is initialized
  socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  return Socket(sock);
}

// Set socket options for low latency
inline void set_tcp_nodelay(socket_t sock, bool enable = true)
{
  int flag = enable ? 1 : 0;
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));
}

inline void set_reuse_addr(socket_t sock, bool enable = true)
{
  int flag = enable ? 1 : 0;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&flag, sizeof(flag));
}

// Aggressive TCP optimizations for high-performance, low-latency 10G networks
inline void optimize_for_low_latency(socket_t sock)
{
  // 1. TCP_NODELAY - Disable Nagle's algorithm (send immediately)
  int flag = 1;
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));
  int buffer_size = 32 * 1024 * 1024;

#if !defined(_WIN32)
  // 2. TCP_QUICKACK - Disable delayed ACKs (Linux only)
  //    Send ACKs immediately instead of waiting up to 40ms
#ifdef TCP_QUICKACK
  flag = 1;
  setsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, (const char*)&flag, sizeof(flag));
#endif

  // 3. Increase send/receive buffer sizes for 10G networks
  //    Large buffers prevent blocking on fast networks
  //    Default is often 128KB-256KB, we want several MB for 10G
  setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&buffer_size, sizeof(buffer_size));
  setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&buffer_size, sizeof(buffer_size));

  // 4. TCP_CORK / TCP_NOPUSH - Control when data is sent
  //    Not used here since we want immediate sends, but can be useful
  //    for batching small writes

  // 5. SO_PRIORITY - Set socket priority for QoS (Linux)
#ifdef SO_PRIORITY
  int priority = 6; // High priority (0-7 scale)
  setsockopt(sock, SOL_SOCKET, SO_PRIORITY, (const char*)&priority, sizeof(priority));
#endif

  // 6. TCP_USER_TIMEOUT - Detect dead connections faster (Linux)
#ifdef TCP_USER_TIMEOUT
  unsigned int timeout_ms = 5000; // 5 seconds instead of default ~15 minutes
  setsockopt(
      sock, IPPROTO_TCP, TCP_USER_TIMEOUT, (const char*)&timeout_ms, sizeof(timeout_ms));
#endif

  // 7. SO_KEEPALIVE with aggressive settings
  flag = 1;
  setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&flag, sizeof(flag));

#ifdef TCP_KEEPIDLE
  // Start probing after 30 seconds of idle
  int keepidle = 30;
  setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, (const char*)&keepidle, sizeof(keepidle));
#endif

#ifdef TCP_KEEPINTVL
  // Probe every 5 seconds
  int keepintvl = 5;
  setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, (const char*)&keepintvl, sizeof(keepintvl));
#endif

#ifdef TCP_KEEPCNT
  // Give up after 3 failed probes
  int keepcnt = 3;
  setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, (const char*)&keepcnt, sizeof(keepcnt));
#endif

#else // Windows-specific optimizations
  // Increase buffer sizes for 10G networks
  setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&buffer_size, sizeof(buffer_size));
  setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&buffer_size, sizeof(buffer_size));

  // Disable send buffering for minimal latency
  flag = 0;
  setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&flag, sizeof(flag));

  // Enable keepalive
  flag = 1;
  setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&flag, sizeof(flag));

  // Windows-specific: Configure TCP keepalive parameters
  struct tcp_keepalive keepalive_settings;
  keepalive_settings.onoff = 1;
  keepalive_settings.keepalivetime = 30000;     // 30 seconds
  keepalive_settings.keepaliveinterval = 5000;  // 5 seconds
  DWORD bytes_returned;
  WSAIoctl(
      sock, SIO_KEEPALIVE_VALS, &keepalive_settings, sizeof(keepalive_settings), nullptr,
      0, &bytes_returned, nullptr, nullptr);
#endif
}

// Moderate optimizations (balanced performance/compatibility)
inline void optimize_for_streaming(socket_t sock)
{
  // TCP_NODELAY for low latency
  int flag = 1;
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));

  // Reasonable buffer sizes (1 MB)
  int buffer_size = 1024 * 1024;
  setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&buffer_size, sizeof(buffer_size));
  setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&buffer_size, sizeof(buffer_size));

  // Basic keepalive
  flag = 1;
  setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&flag, sizeof(flag));
}

// Connect to a remote host
inline bool connect_socket(socket_t sock, const std::string& host, int port)
{
  struct addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo* result = nullptr;
  if(getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result) != 0)
  {
    return false;
  }

  bool success = false;
  for(struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next)
  {
    if(connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen) == 0)
    {
      success = true;
      break;
    }
  }

  freeaddrinfo(result);
  return success;
}

// Bind and listen on a port
inline bool bind_and_listen(socket_t sock, int port, int backlog = 1)
{
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(port));

  if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0)
  {
    return false;
  }

  if(listen(sock, backlog) != 0)
  {
    return false;
  }

  return true;
}

// Accept a connection
inline Socket accept_connection(socket_t listen_sock)
{
  struct sockaddr_in client_addr = {};
  socklen_t client_len = sizeof(client_addr);

  socket_t client_sock
      = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);

  return Socket(client_sock);
}

// Send data with blocking I/O
inline bool send_all(socket_t sock, std::span<const uint8_t> data)
{
  size_t total_sent = 0;
  while(total_sent < data.size())
  {
    const auto remaining = data.size() - total_sent;
#if defined(_WIN32)
    const int chunk = static_cast<int>(std::min(remaining, static_cast<size_t>(INT_MAX)));
#else
    const size_t chunk = remaining;
#endif
    auto sent = send(
        sock, reinterpret_cast<const char*>(data.data() + total_sent),
        chunk, 0);

    if(sent < 0)
    {
#if defined(_WIN32)
      if(WSAGetLastError() == WSAEINTR)
        continue;
#else
      if(errno == EINTR)
        continue;
#endif
      return false;
    }
    if(sent == 0)
      return false;

    total_sent += sent;
  }

  return true;
}

// Re-enable TCP_QUICKACK (Linux only, resets after each ACK)
inline void refresh_quickack([[maybe_unused]] socket_t sock)
{
#if !defined(_WIN32) && defined(TCP_QUICKACK)
  int flag = 1;
  setsockopt(sock, IPPROTO_TCP, TCP_QUICKACK, &flag, sizeof(flag));
#endif
}

// Receive data with blocking I/O
inline bool recv_all(socket_t sock, std::span<uint8_t> buffer)
{
  size_t total_received = 0;
  while(total_received < buffer.size())
  {
    const auto remaining = buffer.size() - total_received;
#if defined(_WIN32)
    // Winsock recv takes int; clamp to INT_MAX for >2GB buffers
    const int chunk = static_cast<int>(std::min(remaining, static_cast<size_t>(INT_MAX)));
#else
    const size_t chunk = remaining;
#endif
    auto received = recv(
        sock, reinterpret_cast<char*>(buffer.data() + total_received),
        chunk, MSG_WAITALL);

    if(received < 0)
    {
#if defined(_WIN32)
      if(WSAGetLastError() == WSAEINTR)
        continue;
#else
      if(errno == EINTR)
        continue;
#endif
      return false;
    }
    if(received == 0)
      return false;

    total_received += received;
  }

  return true;
}

// Scatter-gather I/O using multiple buffers
#if defined(_WIN32)
inline bool send_scatter(socket_t sock, std::span<WSABUF> buffers)
{
  ULONG total_size = 0;
  for(const auto& buf : buffers)
    total_size += buf.len;

  WSABUF* wsabuf = buffers.data();
  DWORD count = static_cast<DWORD>(buffers.size());

  ULONG remaining = total_size;
  while(remaining > 0)
  {
    DWORD bytes_sent = 0;
    if(WSASend(sock, wsabuf, count, &bytes_sent, 0, nullptr, nullptr) == SOCKET_ERROR)
    {
      int err = WSAGetLastError();
      if(err == WSAEINTR)
        continue;
      return false;
    }
    if(bytes_sent == 0)
      return false;

    remaining -= bytes_sent;

    // Advance past fully-sent WSABUF entries
    DWORD sent = bytes_sent;
    while(sent > 0 && count > 0)
    {
      if(sent >= wsabuf->len)
      {
        sent -= wsabuf->len;
        ++wsabuf;
        --count;
      }
      else
      {
        wsabuf->buf += sent;
        wsabuf->len -= sent;
        sent = 0;
      }
    }
  }

  return true;
}
#else
inline bool send_scatter(socket_t sock, std::span<struct iovec> buffers)
{
  size_t total_size = 0;
  for(const auto& buf : buffers)
    total_size += buf.iov_len;

  // Pointer and count into the iovec array, advanced past fully-sent entries
  struct iovec* iov = buffers.data();
  int iovcnt = static_cast<int>(buffers.size());

  size_t remaining = total_size;
  while(remaining > 0)
  {
    ssize_t sent = writev(sock, iov, iovcnt);
    if(sent < 0)
    {
      if(errno == EINTR)
        continue;
      return false;
    }
    if(sent == 0)
      return false;

    remaining -= sent;

    // Advance past fully-sent iovec entries
    while(sent > 0 && iovcnt > 0)
    {
      if(static_cast<size_t>(sent) >= iov->iov_len)
      {
        // This entry was fully sent, skip it
        sent -= iov->iov_len;
        ++iov;
        --iovcnt;
      }
      else
      {
        // Partial send within this entry, adjust base and len
        iov->iov_base = static_cast<char*>(iov->iov_base) + sent;
        iov->iov_len -= sent;
        sent = 0;
      }
    }
  }

  return true;
}
#endif

} // namespace netstream
