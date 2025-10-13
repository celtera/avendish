# NetStream - High-Performance Network Streaming Objects

NetStream provides two avendish objects for streaming buffers of data over TCP/IP networks with maximum performance and cross-platform compatibility.

## Features

- **Cross-platform**: Works on Windows, macOS, and Linux
- **High performance**: Uses platform-specific optimizations
  - Scatter-gather I/O for zero-copy transmission
  - TCP_NODELAY for low latency
  - Lock-free triple buffering for thread safety
- **Thread-safe**: Background I/O threads prevent blocking the audio/processing thread
- **Multiple data formats**: Supports various common formats for graphics and point clouds
- **Simple protocol**: Easy to implement in other languages/environments

## Objects

### NetStreamReceiver

Receives data buffers over the network.

**Inputs:**
- `Port` (int, 1024-65535): TCP port to listen on (default: 9001)
- `Listen` (bool): Toggle to start/stop listening

**Outputs:**
- `Data` (callback, vector<float>): Received data as float array
- `Status` (string): Connection status
- `Connected` (bool): Connection state
- `Format` (string): Current data format
- `Elements` (int): Number of elements in last packet

**Usage:**
1. Set the port number
2. Enable "Listen" toggle
3. Wait for incoming connection
4. Received data will be output as float vectors

### NetStreamSender

Sends data buffers over the network.

**Inputs:**
- `Host` (string): Target hostname or IP address (default: "127.0.0.1")
- `Port` (int, 1024-65535): TCP port to connect to (default: 9001)
- `Format` (enum): Data format for transmission
  - R (u8): Single channel, 8-bit unsigned
  - RGB (u8): RGB, 8-bit unsigned per channel
  - RGBA (u8): RGBA, 8-bit unsigned per channel
  - R (f32): Single channel, 32-bit float
  - RGB (f32): RGB, 32-bit float per channel
  - RGBA (f32): RGBA, 32-bit float per channel
  - XYZ (f32): 3D position, float per axis
  - XYZW (f32): 4D position, float per component
  - XYZRGB (f32): Point cloud with color, all float32
- `Connect` (bool): Toggle to connect/disconnect

**Messages:**
- `Data` (vector<float>): Send a data buffer

**Outputs:**
- `Status` (string): Connection status
- `Connected` (bool): Connection state
- `Sent` (int): Number of packets sent

**Usage:**
1. Set the target host and port
2. Select the data format
3. Enable "Connect" toggle
4. Send data by calling the "Data" message with a vector<float>

## Protocol Specification

### Packet Format

Each packet consists of a header followed by the payload:

```
+------------------+
|  Packet Header   |  (32 bytes)
+------------------+
|    Payload       |  (variable size)
+------------------+
```

### Header Structure

```cpp
struct PacketHeader {
  uint32_t magic;          // 0x4E455453 ("NETS")
  uint32_t version;        // Protocol version (1)
  uint32_t format;         // DataFormat enum value
  uint32_t num_elements;   // Number of data elements
  uint32_t payload_bytes;  // Payload size in bytes
  uint32_t reserved[3];    // Reserved (set to 0)
};
```

All multi-byte integers are in network byte order (big-endian).

### Data Formats

| Format ID | Name | Components | Bytes/Component | Description |
|-----------|------|------------|-----------------|-------------|
| 1 | R_UCHAR | 1 | 1 | Single channel, 8-bit unsigned |
| 2 | RGB_UCHAR | 3 | 1 | RGB color, 8-bit per channel |
| 3 | RGBA_UCHAR | 4 | 1 | RGBA color, 8-bit per channel |
| 4 | R_FLOAT32 | 1 | 4 | Single channel, 32-bit float |
| 5 | RGB_FLOAT32 | 3 | 4 | RGB color, float per channel |
| 6 | RGBA_FLOAT32 | 4 | 4 | RGBA color, float per channel |
| 7 | XYZ_FLOAT32 | 3 | 4 | 3D position, float per axis |
| 8 | XYZW_FLOAT32 | 4 | 4 | 4D position, float per component |
| 9 | XYZRGB_FLOAT32 | 6 | 4 | Point cloud: XYZ + RGB, all float |

### Data Conversion

- **8-bit unsigned (UCHAR)**: Normalized to [0.0, 1.0] when converted to float
  - `float_value = uint8_value / 255.0`
- **32-bit float**: Transmitted as-is (native float representation)

### Example Implementation (Python)

```python
import socket
import struct

def send_netstream_packet(host, port, data_format, data):
    """
    Send a NetStream packet

    Args:
        host: Target hostname
        port: Target port
        data_format: DataFormat enum value (e.g., 4 for R_FLOAT32)
        data: List of float values
    """
    # Create header
    magic = 0x4E455453
    version = 1
    num_elements = len(data) // components_per_format(data_format)
    payload_bytes = len(data) * 4  # Assuming float32

    # Pack header (big-endian)
    header = struct.pack('!IIIII', magic, version, data_format,
                         num_elements, payload_bytes)
    header += b'\x00' * 12  # reserved

    # Pack payload
    payload = struct.pack(f'!{len(data)}f', *data)

    # Send
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    sock.sendall(header + payload)
    sock.close()
```

## Architecture

### Thread Safety

The implementation uses a lock-free triple buffer design for communication between the I/O thread and the processing thread:

- **Write buffer**: I/O thread writes here
- **Read buffer**: Processing thread reads here
- **Swap buffer**: Used for atomic swapping

This design ensures:
- No locks in the critical path
- No blocking operations in the processing thread
- Minimal latency for data transfer

### Cross-Platform I/O

The `NetIO.hpp` abstraction layer provides:

- **Windows**: WinSock2 API with potential for IOCP (future enhancement)
- **Linux**: BSD sockets with potential for io_uring (future enhancement)
- **macOS**: BSD sockets

Current implementation uses blocking I/O in background threads, which provides excellent performance while maintaining simplicity. Future versions could leverage:
- Windows IOCP for scalable I/O
- Linux io_uring for highest performance
- macOS kqueue for efficient event notification

### Scatter-Gather I/O

The sender uses scatter-gather I/O to send the header and payload in a single system call:

- **Windows**: `WSASend()` with WSABUF array
- **Linux/macOS**: `writev()` with iovec array

This minimizes CPU overhead and maximizes throughput.

## Performance Considerations

1. **TCP_NODELAY**: Enabled by default to disable Nagle's algorithm for low latency
2. **SO_REUSEADDR**: Enabled on listen socket for quick restart
3. **Lock-free design**: Triple buffer uses atomic operations only
4. **Zero-copy**: Scatter-gather I/O avoids buffer copies
5. **Background threads**: I/O never blocks the processing thread

## Limitations and Future Work

1. **Single connection**: Each receiver accepts one connection at a time
2. **TCP only**: UDP support could be added for lower latency (with packet loss)
3. **No compression**: Data is sent uncompressed
4. **No encryption**: Consider TLS for secure transmission
5. **Basic error handling**: Could be enhanced with automatic reconnection
6. **Platform I/O**: Could leverage IOCP/io_uring for even better performance

## Building

The NetStream objects are header-only and require:

- C++20 compiler
- Platform networking libraries:
  - Windows: ws2_32.lib (automatically linked)
  - Linux/macOS: No additional libraries needed

Include in your avendish build by adding the files to your project.

## License

GPL-3.0-or-later

## Author

Jean-Michaël Celerier
