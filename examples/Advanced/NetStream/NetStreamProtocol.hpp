#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace netstream
{

// Network byte order conversion utilities
inline uint32_t htonl_portable(uint32_t hostlong)
{
#if defined(_WIN32)
  return ((hostlong & 0xFF000000) >> 24) | ((hostlong & 0x00FF0000) >> 8)
         | ((hostlong & 0x0000FF00) << 8) | ((hostlong & 0x000000FF) << 24);
#else
  return __builtin_bswap32(hostlong);
#endif
}

inline uint32_t ntohl_portable(uint32_t netlong)
{
  return htonl_portable(netlong); // Same operation
}

// Data format enumeration
enum class DataFormat : uint32_t
{
  UNKNOWN = 0,
  R_UCHAR = 1,        // Single channel, 8-bit unsigned
  RGB_UCHAR = 2,      // RGB, 8-bit unsigned per channel
  RGBA_UCHAR = 3,     // RGBA, 8-bit unsigned per channel
  R_FLOAT32 = 4,      // Single channel, 32-bit float
  RGB_FLOAT32 = 5,    // RGB, 32-bit float per channel
  RGBA_FLOAT32 = 6,   // RGBA, 32-bit float per channel
  XYZ_FLOAT32 = 7,    // 3D position, 32-bit float per axis
  XYZW_FLOAT32 = 8,   // 4D position, 32-bit float per component
  XYZRGB_FLOAT32 = 9, // Point cloud with color, all float32
  // Future formats can be added here
};

// Protocol packet header
struct PacketHeader
{
  static constexpr uint32_t MAGIC = 0xC0A0F0E0; // "NETS" in ASCII
  static constexpr uint32_t VERSION = 1;

  uint32_t magic{MAGIC};        // Protocol magic number
  uint32_t version{VERSION};    // Protocol version
  uint32_t format;              // DataFormat enum value
  uint32_t num_elements;        // Number of data elements
  uint32_t payload_bytes;       // Total payload size in bytes
  uint32_t reserved[3]{0, 0, 0}; // Reserved for future use

  // Convert to network byte order
  void to_network_order()
  {
    magic = htonl_portable(magic);
    version = htonl_portable(version);
    format = htonl_portable(format);
    num_elements = htonl_portable(num_elements);
    payload_bytes = htonl_portable(payload_bytes);
    for(auto& r : reserved)
      r = htonl_portable(r);
  }

  // Convert from network byte order
  void to_host_order()
  {
    magic = ntohl_portable(magic);
    version = ntohl_portable(version);
    format = ntohl_portable(format);
    num_elements = ntohl_portable(num_elements);
    payload_bytes = ntohl_portable(payload_bytes);
    for(auto& r : reserved)
      r = ntohl_portable(r);
  }

  // Validate header
  bool is_valid() const
  {
    return magic == MAGIC && version == VERSION && format > 0
           && num_elements > 0 && payload_bytes > 0;
  }
};

// Get the number of components per element for a format
inline constexpr int components_per_element(DataFormat fmt)
{
  switch(fmt)
  {
    case DataFormat::R_UCHAR:
    case DataFormat::R_FLOAT32:
      return 1;
    case DataFormat::RGB_UCHAR:
    case DataFormat::RGB_FLOAT32:
    case DataFormat::XYZ_FLOAT32:
      return 3;
    case DataFormat::RGBA_UCHAR:
    case DataFormat::RGBA_FLOAT32:
    case DataFormat::XYZW_FLOAT32:
      return 4;
    case DataFormat::XYZRGB_FLOAT32:
      return 6;
    default:
      return 0;
  }
}

// Get the bytes per component for a format
inline constexpr int bytes_per_component(DataFormat fmt)
{
  switch(fmt)
  {
    case DataFormat::R_UCHAR:
    case DataFormat::RGB_UCHAR:
    case DataFormat::RGBA_UCHAR:
      return 1;
    case DataFormat::R_FLOAT32:
    case DataFormat::RGB_FLOAT32:
    case DataFormat::RGBA_FLOAT32:
    case DataFormat::XYZ_FLOAT32:
    case DataFormat::XYZW_FLOAT32:
    case DataFormat::XYZRGB_FLOAT32:
      return 4;
    default:
      return 0;
  }
}

// Convert raw data to float vector
inline void convert_to_float(
    DataFormat fmt, std::span<const uint8_t> data, std::vector<float>& output)
{
  const int comp_per_elem = components_per_element(fmt);
  const int bytes_per_comp = bytes_per_component(fmt);

  if(comp_per_elem == 0 || bytes_per_comp == 0)
  {
    output.clear();
    return;
  }

  const size_t total_components = data.size() / bytes_per_comp;
  output.resize(total_components);

  if(bytes_per_comp == 1) // unsigned char formats
  {
    for(size_t i = 0; i < total_components; ++i)
    {
      output[i] = static_cast<float>(data[i]) / 255.0f;
    }
  }
  else if(bytes_per_comp == 4) // float32 formats
  {
    std::memcpy(output.data(), data.data(), data.size());
  }
}

// Convert float vector to raw data
inline std::vector<uint8_t> convert_from_float(DataFormat fmt, std::span<const float> data)
{
  const int comp_per_elem = components_per_element(fmt);
  const int bytes_per_comp = bytes_per_component(fmt);

  if(comp_per_elem == 0 || bytes_per_comp == 0)
  {
    return {};
  }

  std::vector<uint8_t> output;

  if(bytes_per_comp == 1) // unsigned char formats
  {
    output.resize(data.size());
    for(size_t i = 0; i < data.size(); ++i)
    {
      float clamped = std::clamp(data[i], 0.0f, 1.0f);
      output[i] = static_cast<uint8_t>(clamped * 255.0f);
    }
  }
  else if(bytes_per_comp == 4) // float32 formats
  {
    output.resize(data.size() * sizeof(float));
    std::memcpy(output.data(), data.data(), output.size());
  }

  return output;
}

} // namespace netstream
