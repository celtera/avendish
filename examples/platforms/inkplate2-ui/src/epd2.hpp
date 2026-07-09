#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

// Minimal driver for the Inkplate 2's 2.13" 212x104 black/white/red
// e-paper (UC8151-class controller on VSPI). Self-contained so the demo
// does not depend on the Inkplate Arduino library (whose bundled
// NetworkClient collides with Arduino core 3.x). Command sequence and pin
// map per Soldered's Inkplate2 driver (LGPLv3), re-implemented.
//
// Layout: two bitplanes, native portrait 104x212, MSB-first, 13 bytes/row.
// Plane 0: 1 = white, 0 = black. Plane 1: 0 = red (overrides).

#include <Arduino.h>
#include <SPI.h>

namespace epd2
{
inline constexpr int native_w = 104, native_h = 212;
inline constexpr int plane_bytes = native_w * native_h / 8;

inline constexpr int pin_rst = 19, pin_dc = 33, pin_cs = 15, pin_busy = 32;
inline constexpr int pin_clk = 18, pin_din = 23;

enum ink : uint8_t
{
  white = 0,
  black = 1,
  red = 2
};

inline SPIClass spi{VSPI};
inline uint8_t plane_bw[plane_bytes];
inline uint8_t plane_red[plane_bytes];

inline void command(uint8_t c)
{
  digitalWrite(pin_cs, LOW);
  digitalWrite(pin_dc, LOW);
  delayMicroseconds(10);
  spi.beginTransaction(SPISettings(1000000UL, MSBFIRST, SPI_MODE0));
  spi.transfer(c);
  spi.endTransaction();
  digitalWrite(pin_cs, HIGH);
  delay(1);
}

inline void data(const uint8_t* d, int n)
{
  digitalWrite(pin_cs, LOW);
  digitalWrite(pin_dc, HIGH);
  delayMicroseconds(10);
  spi.beginTransaction(SPISettings(1000000UL, MSBFIRST, SPI_MODE0));
  spi.writeBytes(d, n);
  spi.endTransaction();
  digitalWrite(pin_cs, HIGH);
  delay(1);
}

inline void data(uint8_t d)
{
  data(&d, 1);
}

inline bool wait_idle(uint32_t timeout_ms)
{
  const auto t0 = millis();
  while(!digitalRead(pin_busy) && millis() - t0 < timeout_ms)
    ;
  if(!digitalRead(pin_busy))
    return false;
  delay(200);
  return true;
}

inline bool wake()
{
  spi.begin(pin_clk, -1, pin_din, -1);
  pinMode(pin_cs, OUTPUT);
  pinMode(pin_dc, OUTPUT);
  pinMode(pin_rst, OUTPUT);
  pinMode(pin_busy, INPUT_PULLUP);
  delay(10);

  digitalWrite(pin_rst, LOW);
  delay(100);
  digitalWrite(pin_rst, HIGH);
  delay(100);

  command(0x04); // power on
  if(!wait_idle(1000))
    return false;

  command(0x00); // panel setting
  data(0x0f);    // LUT from OTP
  data(0x89);

  command(0x61); // resolution
  data(native_w);
  data(native_h >> 8);
  data(native_h & 0xff);

  command(0x50); // VCOM / data interval
  data(0x77);
  return true;
}

inline void sleep()
{
  command(0x50);
  data(0xf7);
  command(0x02); // power off
  wait_idle(1000);
  command(0x07); // deep sleep
  data(0xA5);
  delay(1);
  spi.end();
  for(int p : {pin_rst, pin_dc, pin_cs, pin_busy, pin_clk, pin_din})
    pinMode(p, INPUT);
}

inline void clear()
{
  memset(plane_bw, 0xff, plane_bytes);
  memset(plane_red, 0xff, plane_bytes);
}

// Landscape coordinates (x in [0,212), y in [0,104)), rotated onto the
// portrait-native panel.
inline void set_pixel(int x, int y, ink color)
{
  const int nx = y;
  const int ny = native_h - 1 - x;
  if(nx < 0 || nx >= native_w || ny < 0 || ny >= native_h)
    return;
  const int index = ny * (native_w / 8) + nx / 8;
  const uint8_t mask = uint8_t(0x80 >> (nx % 8));
  if(color == black)
    plane_bw[index] &= ~mask;
  else if(color == red)
    plane_red[index] &= ~mask;
}

// Push both planes and refresh (~15 s), then put the panel back to sleep.
inline bool show()
{
  if(!wake())
    return false;
  delay(20);
  command(0x10);
  data(plane_bw, plane_bytes);
  command(0x13);
  data(plane_red, plane_bytes);
  command(0x11);
  data(0x00);
  command(0x12); // refresh
  delayMicroseconds(500);
  const bool ok = wait_idle(60000);
  sleep();
  return ok;
}
}
