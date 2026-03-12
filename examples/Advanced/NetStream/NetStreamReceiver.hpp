#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "NetIO.hpp"
#include "NetStreamProtocol.hpp"
#include "TripleBuffer.hpp"

#include <cmath>
#include <halp/buffer.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <atomic>
#include <thread>
#include <vector>

namespace netstream
{

struct NetStreamReceiverBase
{
  void start_listening(this auto& self)
  {
    if(self.m_running.load(std::memory_order_acquire))
      return;

    self.m_running.store(true, std::memory_order_release);
    self.m_thread = std::thread([&self]() { self.receiver_thread(); });

    self.outputs.status.value = "Starting...";
  }

  void stop_listening(this auto& self)
  {
    self.m_running.store(false, std::memory_order_release);

    if(self.m_thread.joinable())
    {
      // Shutdown unblocks any thread stuck in recv()/accept()
      self.m_listen_socket.shutdown();
      self.m_client_socket.shutdown();

      self.m_thread.join();

      // Now safe to close after the thread has exited
      self.m_listen_socket.close();
      self.m_client_socket.close();
    }

    self.outputs.status.value = "Stopped";
    self.outputs.connected.value = false;
  }

  void receiver_thread(this auto& self)
  {
    try
    {
      // Create listen socket
      self.m_listen_socket = create_tcp_socket();
      if(!self.m_listen_socket.is_valid())
      {
        self.m_status_message = "Failed to create socket";
        return;
      }

      set_reuse_addr(self.m_listen_socket.native_handle());

      // Bind and listen
      if(!bind_and_listen(self.m_listen_socket.native_handle(), self.inputs.port))
      {
        self.m_status_message
            = "Failed to bind port " + std::to_string(self.inputs.port.value);
        return;
      }

      self.m_status_message
          = "Listening on port " + std::to_string(self.inputs.port.value);

      while(self.m_running.load(std::memory_order_acquire))
      {
        // Accept connection
        self.m_status_message = "Waiting for connection...";

        // Use non-blocking accept with timeout to allow checking m_running
        set_non_blocking(self.m_listen_socket.native_handle());

        Socket client;
        while(self.m_running.load(std::memory_order_acquire))
        {
          client = accept_connection(self.m_listen_socket.native_handle());
          if(client.is_valid())
            break;

          // Sleep briefly to avoid busy-waiting
          std::this_thread::sleep_for(std::chrono::microseconds(1000));
        }

        if(!client.is_valid())
          break;

        // Set client socket to blocking mode for easier I/O
        set_blocking(client.native_handle());
        optimize_for_low_latency(client.native_handle());

        self.m_client_socket = std::move(client);
        self.m_connected.store(true, std::memory_order_release);
        self.m_status_message = "Client connected";

        // Receive data loop
        while(self.m_running.load(std::memory_order_acquire)
              && self.m_client_socket.is_valid())
        {
          bool failed = false;
          switch(self.inputs.protocol.value) // FIXME need atomic read
          {
            case ProtocolType::Multi: {
              if(!self.receive_packet_multi())
              {
                failed = true;
                break;
              }
              break;
            }
            case ProtocolType::Raw_XYZ_F32: {
              if(!self.receive_packet_xyz_f32())
              {
                failed = true;
                break;
              }
              break;
            }
          }
          if(failed)
            break;

          refresh_quickack(self.m_client_socket.native_handle());
        }

        self.m_client_socket.close();
        self.m_connected.store(false, std::memory_order_release);
        self.m_status_message = "Client disconnected";
      }
    }
    catch(const std::exception& e)
    {
      self.m_status_message = std::string("Error: ") + e.what();
    }

    self.m_running.store(false, std::memory_order_release);
  }

  bool receive_packet_multi()
  {
    // Receive header
    PacketHeader header;
    std::span<uint8_t> header_span(reinterpret_cast<uint8_t*>(&header), sizeof(header));

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

    auto fmt = static_cast<DataFormat>(header.format);

    if(fmt == DataFormat::RAW)
    {
      // Fast path: receive directly into the triple buffer, zero intermediate copies
      const size_t num_floats = header.payload_bytes / sizeof(float);
      auto& write_buf = m_triple_buffer.write_buffer();
      write_buf.resize(num_floats);

      std::span<uint8_t> dst(
          reinterpret_cast<uint8_t*>(write_buf.data()), header.payload_bytes);
      if(!recv_all(m_client_socket.native_handle(), dst))
        return false;
    }
    else
    {
      // Conversion path: receive into byte storage, then convert
      m_payload_storage.resize(header.payload_bytes);
      if(!recv_all(m_client_socket.native_handle(), std::span(m_payload_storage)))
        return false;

      convert_to_float(fmt, m_payload_storage, m_float_data_storage);

      auto& write_buf = m_triple_buffer.write_buffer();
      write_buf.assign(m_float_data_storage.begin(), m_float_data_storage.end());
    }

    // Update format info
    m_format = format_to_string(fmt) + " (" + std::to_string(header.num_elements)
               + " elements)";

    m_triple_buffer.publish();

    return true;
  }

  bool receive_packet_xyz_f32()
  {
    // Receive header
    XYZFloat32PacketHeader header;
    std::span<uint8_t> header_span(reinterpret_cast<uint8_t*>(&header), sizeof(header));

    if(!recv_all(m_client_socket.native_handle(), header_span))
    {
      return false;
    }

    // Little-endian here

    // Validate header
    if(!header.is_valid())
    {
      m_status_message = "Invalid packet header";
      return false;
    }

    {
      // Receive directly into the triple buffer, zero intermediate copies
      const size_t num_floats = header.payload_bytes / sizeof(float);
      auto& write_buf = m_triple_buffer.write_buffer();
      write_buf.resize(num_floats);

      std::span<uint8_t> dst(
          reinterpret_cast<uint8_t*>(write_buf.data()), header.payload_bytes);
      if(!recv_all(m_client_socket.native_handle(), dst))
        return false;
    }

    // Update format info
    m_format = "XYZ F32";

    m_triple_buffer.publish();

    return true;
  }

  void update_status(this auto& self)
  {
    //outputs.status.value = m_status_message;
    self.outputs.connected.value = self.m_connected.load(std::memory_order_acquire);
    self.outputs.format.value = self.m_format;
  }

  static std::string format_to_string(DataFormat fmt)
  {
    switch(fmt)
    {
      case DataFormat::RAW:
        return "Raw";
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
  TripleBuffer<float> m_triple_buffer;
  std::string m_status_message{"Idle"};
  std::string m_format;
  std::vector<uint8_t> m_payload_storage;
  std::vector<float> m_float_data_storage;
};

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
class NetStreamReceiver : public NetStreamReceiverBase
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

    halp::combobox_t<"Protocol", ProtocolType> protocol;

    halp::toggle<"Listen"> listen;
  } inputs;

  struct outputs_t
  {
    // span<raw_bytes> instead?
    halp::val_port<"Data", std::vector<float>> data;

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
    if(m_triple_buffer.consume())
    {
      auto& buf = m_triple_buffer.read_buffer();
      outputs.data.value.assign(buf.begin(), buf.end());
      // Update outputs
      outputs.elements.value = static_cast<int>(outputs.data.value.size());
    }

    // Update status from atomic flags
    update_status();
  }
};

class NetStreamReceiver_GPU : public NetStreamReceiverBase
{
public:
  halp_meta(name, "NetStream Receiver (GPU)")
  halp_meta(c_name, "netstream_receiver_gpu")
  halp_meta(category, "Network")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Receives data buffers over the network")
  halp_meta(uuid, "8334c3f2-b984-4d19-880c-7fb1f0b10cfe")

  struct inputs_t
  {
    halp::spinbox_i32<"Port", halp::range{1024, 65535, 9001}> port;

    halp::combobox_t<"Protocol", ProtocolType> protocol;

    halp::toggle<"Listen"> listen;
  } inputs;

  struct outputs_t
  {
    // span<raw_bytes> instead?
    halp::cpu_buffer_output<"Data"> data;

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

  NetStreamReceiver_GPU() { m_running.store(false, std::memory_order_relaxed); }

  ~NetStreamReceiver_GPU() { stop_listening(); }

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
    if(m_triple_buffer.consume())
    {
      auto& buf = m_triple_buffer.read_buffer();
      // Update outputs
      const auto N = static_cast<int>(buf.size());
      outputs.elements.value = N;
      outputs.data.buffer.upload((const char*)buf.data(), 0, N);
    }

    // Update status from atomic flags
    update_status();
  }
};
} // namespace netstream
