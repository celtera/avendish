#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <system_error>

// Platform-specific includes
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
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
    int sent = send(
        sock, reinterpret_cast<const char*>(data.data() + total_sent),
        static_cast<int>(data.size() - total_sent), 0);

    if(sent <= 0)
    {
      return false;
    }

    total_sent += sent;
  }

  return true;
}

// Receive data with blocking I/O
inline bool recv_all(socket_t sock, std::span<uint8_t> buffer)
{
  size_t total_received = 0;
  while(total_received < buffer.size())
  {
    int received = recv(
        sock, reinterpret_cast<char*>(buffer.data() + total_received),
        static_cast<int>(buffer.size() - total_received), 0);

    if(received <= 0)
    {
      return false;
    }

    total_received += received;
  }

  return true;
}

// Scatter-gather I/O using multiple buffers
#if defined(_WIN32)
inline bool send_scatter(socket_t sock, std::span<WSABUF> buffers)
{
  DWORD bytes_sent = 0;
  if(WSASend(sock, buffers.data(), (DWORD)buffers.size(), &bytes_sent, 0, nullptr, nullptr)
     == SOCKET_ERROR)
  {
    return false;
  }
  return true;
}
#else
inline bool send_scatter(socket_t sock, std::span<struct iovec> buffers)
{
  ssize_t total_sent = 0;
  size_t total_size = 0;
  for(const auto& buf : buffers)
  {
    total_size += buf.iov_len;
  }

  while(total_sent < total_size)
  {
    ssize_t sent = writev(sock, buffers.data(), buffers.size());
    if(sent <= 0)
    {
      return false;
    }
    total_sent += sent;

    // If not all data was sent, we'd need to adjust the buffers
    // For simplicity, we assume writev sends all data in one call
    break;
  }

  return total_sent == total_size;
}
#endif

} // namespace netstream
