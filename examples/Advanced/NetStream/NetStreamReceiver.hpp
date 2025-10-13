#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "NetIO.hpp"
#include "NetStreamProtocol.hpp"
#include "TripleBuffer.hpp"

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

namespace netstream
{

/**
 * NetStreamReceiver - Receives buffer data over the network
 *
 * This object opens a TCP port and waits for incoming connections.
 * When data is received, it's converted to std::vector<float> and
 * made available as an output.
 *
 * The receiving happens in a background thread to avoid blocking
 * the audio/processing thread.
 */
class NetStreamReceiver
{
public:
  halp_meta(name, "NetStream Receiver")
  halp_meta(c_name, "netstream_receiver")
  halp_meta(category, "Network")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Receives data buffers over the network")
  halp_meta(uuid, "a7f8e3d2-5c4b-4a1e-9f6d-8b2c3a1e7d4f")

  struct inputs_t
  {
    halp::spinbox_i32<"Port", halp::range{1024, 65535, 9001}> port;
    halp::toggle<"Listen"> listen;
  } inputs;

  struct outputs_t
  {
    // span<raw_bytes> instead?
    halp::callback<"Data", std::vector<float>> data;

    struct : halp::val_port<"Status", std::string>
    {
      std::string value{"Idle"};
    } status;

    struct : halp::val_port<"Connected", bool>
    {
      bool value{false};
    } connected;

    struct : halp::val_port<"Format", std::string>
    {
      std::string value{};
    } format;

    struct : halp::val_port<"Elements", int>
    {
      int value{0};
    } elements;
  } outputs;

  NetStreamReceiver()
  {
    m_running.store(false, std::memory_order_relaxed);
  }

  ~NetStreamReceiver() { stop_listening(); }

  void operator()()
  {
    // Check if we need to start/stop the receiver
    if(inputs.listen && !m_running.load(std::memory_order_acquire))
    {
      start_listening();
    }
    else if(!inputs.listen && m_running.load(std::memory_order_acquire))
    {
      stop_listening();
    }

    // Check for new data from the I/O thread
    std::vector<float> new_data;
    if(m_triple_buffer.read(new_data))
    {
      // Update outputs
      outputs.elements.value = static_cast<int>(new_data.size());
      outputs.data(std::move(new_data));
    }

    // Update status from atomic flags
    update_status();
  }

private:
  void start_listening()
  {
    if(m_running.load(std::memory_order_acquire))
      return;

    m_running.store(true, std::memory_order_release);
    m_thread = std::thread([this]() { this->receiver_thread(); });

    outputs.status.value = "Starting...";
  }

  void stop_listening()
  {
    m_running.store(false, std::memory_order_release);

    if(m_thread.joinable())
    {
      // Close sockets to unblock accept/recv calls
      m_listen_socket.close();
      m_client_socket.close();

      m_thread.join();
    }

    outputs.status.value = "Stopped";
    outputs.connected.value = false;
  }

  void receiver_thread()
  {
    try
    {
      // Create listen socket
      m_listen_socket = create_tcp_socket();
      if(!m_listen_socket.is_valid())
      {
        m_status_message = "Failed to create socket";
        return;
      }

      set_reuse_addr(m_listen_socket.native_handle());

      // Bind and listen
      if(!bind_and_listen(m_listen_socket.native_handle(), inputs.port))
      {
        m_status_message = "Failed to bind port " + std::to_string(inputs.port.value);
        return;
      }

      m_status_message = "Listening on port " + std::to_string(inputs.port.value);

      while(m_running.load(std::memory_order_acquire))
      {
        // Accept connection
        m_status_message = "Waiting for connection...";

        // Use non-blocking accept with timeout to allow checking m_running
        set_non_blocking(m_listen_socket.native_handle());

        Socket client;
        while(m_running.load(std::memory_order_acquire))
        {
          client = accept_connection(m_listen_socket.native_handle());
          if(client.is_valid())
            break;

          // Sleep briefly to avoid busy-waiting
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if(!client.is_valid())
          break;

        // Set client socket to blocking mode for easier I/O
        set_blocking(client.native_handle());
        set_tcp_nodelay(client.native_handle());

        m_client_socket = std::move(client);
        m_connected.store(true, std::memory_order_release);
        m_status_message = "Client connected";

        // Receive data loop
        while(m_running.load(std::memory_order_acquire)
              && m_client_socket.is_valid())
        {
          if(!receive_packet())
          {
            break;
          }
        }

        m_client_socket.close();
        m_connected.store(false, std::memory_order_release);
        m_status_message = "Client disconnected";
      }
    }
    catch(const std::exception& e)
    {
      m_status_message = std::string("Error: ") + e.what();
    }

    m_running.store(false, std::memory_order_release);
  }

  bool receive_packet()
  {
    // Receive header
    PacketHeader header;
    std::span<uint8_t> header_span(
        reinterpret_cast<uint8_t*>(&header), sizeof(header));

    if(!recv_all(m_client_socket.native_handle(), header_span))
    {
      return false;
    }

    // Convert to host byte order
    header.to_host_order();

    // Validate header
    if(!header.is_valid())
    {
      m_status_message = "Invalid packet header";
      return false;
    }

    // Receive payload
    std::vector<uint8_t> payload(header.payload_bytes);
    if(!recv_all(m_client_socket.native_handle(), std::span(payload)))
    {
      return false;
    }

    // Convert to float
    std::vector<float> float_data;
    convert_to_float(static_cast<DataFormat>(header.format), payload, float_data);

    // Update format info
    m_format
        = format_to_string(static_cast<DataFormat>(header.format)) + " ("
          + std::to_string(header.num_elements) + " elements)";

    // Write to triple buffer
    m_triple_buffer.write(std::move(float_data));

    return true;
  }

  void update_status()
  {
    outputs.status.value = m_status_message;
    outputs.connected.value = m_connected.load(std::memory_order_acquire);
    outputs.format.value = m_format;
  }

  static std::string format_to_string(DataFormat fmt)
  {
    switch(fmt)
    {
      case DataFormat::R_UCHAR:
        return "R (u8)";
      case DataFormat::RGB_UCHAR:
        return "RGB (u8)";
      case DataFormat::RGBA_UCHAR:
        return "RGBA (u8)";
      case DataFormat::R_FLOAT32:
        return "R (f32)";
      case DataFormat::RGB_FLOAT32:
        return "RGB (f32)";
      case DataFormat::RGBA_FLOAT32:
        return "RGBA (f32)";
      case DataFormat::XYZ_FLOAT32:
        return "XYZ (f32)";
      case DataFormat::XYZW_FLOAT32:
        return "XYZW (f32)";
      case DataFormat::XYZRGB_FLOAT32:
        return "XYZRGB (f32)";
      default:
        return "Unknown";
    }
  }

  std::atomic<bool> m_running{false};
  std::atomic<bool> m_connected{false};
  std::thread m_thread;
  Socket m_listen_socket;
  Socket m_client_socket;
  TripleBuffer<std::vector<float>> m_triple_buffer;
  std::string m_status_message{"Idle"};
  std::string m_format;
};

} // namespace netstream
