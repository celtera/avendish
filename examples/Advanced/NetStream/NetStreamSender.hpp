#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "NetIO.hpp"
#include "NetStreamProtocol.hpp"
#include "TripleBuffer.hpp"

#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <atomic>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

namespace netstream
{

// TODO https://github.com/burtscher/FPcompress/

/**
 * NetStreamSender - Sends buffer data over the network
 *
 * This object connects to a remote host and sends data buffers.
 * Data is sent via messages (std::vector<float>) and transmitted
 * in a background thread to avoid blocking the audio/processing thread.
 *
 * Uses scatter-gather I/O for efficient transmission.
 */
class NetStreamSender
{
public:
  halp_meta(name, "NetStream Sender")
  halp_meta(c_name, "netstream_sender")
  halp_meta(category, "Network")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(description, "Sends FP data buffers over the network")
  halp_meta(uuid, "b8e9f4c3-6d5a-4b2f-8e7c-9a3d4b2e8c5f")

  struct inputs_t
  {
    halp::lineedit<"Host", "127.0.0.1"> host;

    halp::spinbox_i32<"Port", halp::range{1024, 65535, 9001}> port;

    halp::toggle<"Connect"> connect;
  } inputs;

  struct outputs_t
  {
    struct : halp::val_port<"Status", std::string>
    {
      std::string value{"Disconnected"};
    } status;

    struct : halp::val_port<"Connected", bool>
    {
      bool value{false};
    } connected;

    struct : halp::val_port<"Sent", int>
    {
      int value{0};
    } packets_sent;
  } outputs;

  struct messages_t
  {
    struct
    {
      halp_meta(name, "Data")
      void operator()(NetStreamSender& self, std::vector<float> data)
      {
        // Queue data for sending in the background thread
        if(self.m_connected.load(std::memory_order_acquire))
        {
          self.m_triple_buffer.write(std::move(data));
        }
      }
    } data;
  } messages;

  NetStreamSender()
  {
    m_running.store(false, std::memory_order_relaxed);
    m_connected.store(false, std::memory_order_relaxed);
  }

  ~NetStreamSender() { disconnect(); }

  void operator()()
  {
    // Check if we need to connect/disconnect
    if(inputs.connect && !m_running.load(std::memory_order_acquire))
    {
      start_connection();
    }
    else if(!inputs.connect && m_running.load(std::memory_order_acquire))
    {
      disconnect();
    }
    else if(m_thread_finished.load(std::memory_order_acquire))
    {
      disconnect();
    }

    // Update status from atomic flags
    update_status();
  }

private:
  void start_connection()
  {
    if(m_running.load(std::memory_order_acquire))
      return;

    m_running.store(true, std::memory_order_release);
    m_thread = std::thread([this]() { this->sender_thread(); });

    outputs.status.value = "Connecting...";
  }

  void disconnect()
  {
    m_running.store(false, std::memory_order_release);
    m_thread_finished.store(false, std::memory_order_release);

    if(m_thread.joinable())
    {
      m_socket.close();
      m_thread.join();
    }

    outputs.status.value = "Disconnected";
    outputs.connected.value = false;
    m_connected.store(false, std::memory_order_release);
  }

  void sender_thread()
  {
    try
    {
      // Create socket
      m_socket = create_tcp_socket();
      if(!m_socket.is_valid())
      {
        m_status_message = "Failed to create socket";
        std::cerr << "SEND: " <<  m_status_message << "\n";
        return;
      }

      optimize_for_low_latency(m_socket.native_handle());

      // Connect to host
      m_status_message = "Connecting to " + inputs.host.value + ":"
                         + std::to_string(inputs.port.value);
      std::cerr << "SEND: " << m_status_message << "\n";

      if(!connect_socket(m_socket.native_handle(), inputs.host.value, inputs.port))
      {
        m_status_message = "Failed to connect";
        std::cerr  << "SEND: "<< m_status_message << "\n";
        m_socket.close();
        return;
      }

      m_connected.store(true, std::memory_order_release);
      m_status_message = "Connected to " + inputs.host.value + ":"
                         + std::to_string(inputs.port.value);
      std::cerr  << "SEND: "<< m_status_message << "\n";

      // Send loop
      int packets_sent = 0;
      while(m_running.load(std::memory_order_acquire))
      {
        // Check for data to send
        if(m_triple_buffer.new_data_available())
        {
          const auto& data = m_triple_buffer.read_ref();

          if(!send_packet(data))
          {
            m_status_message = "Send failed";
            std::cerr << "SEND: " << m_status_message << "\n";
            break;
          }

          packets_sent++;
          m_packets_sent.store(packets_sent, std::memory_order_relaxed);
        }
        else
        {
          // No data to send, sleep briefly
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }

      m_socket.close();
      m_connected.store(false, std::memory_order_release);
      m_status_message = "Disconnected";
      std::cerr << "SEND: " << m_status_message << "\n";
    }
    catch(const std::exception& e)
    {
      m_status_message = std::string("Error: ") + e.what();
      std::cerr  << "SEND: " << m_status_message << "\n";
      m_connected.store(false, std::memory_order_release);
    }

    m_thread_finished.store(true, std::memory_order_release);
  }

  bool send_packet(const std::vector<float>& payload)
  {
    if(payload.empty())
      return true;

    // Convert format enum to DataFormat
    DataFormat fmt = DataFormat::RAW; //index_to_format(inputs.format.value);

    // Calculate number of elements
    int comp_per_elem = components_per_element(fmt);
    uint32_t num_elements
        = comp_per_elem > 0 ? static_cast<uint32_t>(payload.size() / comp_per_elem) : 0;

    // Create header
    PacketHeader header;
    header.format = static_cast<uint32_t>(fmt);
    header.num_elements = payload.size(); // num_elements;
    header.payload_bytes = static_cast<uint32_t>(payload.size()) * sizeof(float);

    // Convert to network byte order
    header.to_network_order();

    // Send using scatter-gather I/O
#if defined(_WIN32)
    WSABUF buffers[2];
    buffers[0].buf = reinterpret_cast<char*>(&header);
    buffers[0].len = sizeof(header);
    buffers[1].buf = const_cast<char*>(reinterpret_cast<const char*>(payload.data()));
    buffers[1].len = static_cast<ULONG>(payload.size())  * sizeof(float);

    return send_scatter(m_socket.native_handle(), std::span(buffers, 2));
#else
    struct iovec buffers[2];
    buffers[0].iov_base = &header;
    buffers[0].iov_len = sizeof(header);
    buffers[1].iov_base = payload.data();
    buffers[1].iov_len = payload.size();

    return send_scatter(m_socket.native_handle(), std::span(buffers, 2));
#endif
  }

  void update_status()
  {
   // outputs.status.value = m_status_message;
    outputs.connected.value = m_connected.load(std::memory_order_acquire);
    outputs.packets_sent.value = m_packets_sent.load(std::memory_order_relaxed);
  }

  static DataFormat index_to_format(int index)
  {
    return static_cast<DataFormat>(index + 1);
  }

  std::thread m_thread;
  Socket m_socket;
  TripleBuffer<std::vector<float>> m_triple_buffer;
  std::string m_status_message{"Disconnected"};

  alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_running{false};
  alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_connected{false};
  alignas(std::hardware_destructive_interference_size) std::atomic<bool> m_thread_finished{false};
  alignas(std::hardware_destructive_interference_size) std::atomic<int> m_packets_sent{0};
};

} // namespace netstream
