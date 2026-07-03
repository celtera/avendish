# Avendish soft UI on ESP32

Runs a plug-in's declarative `struct ui` (and custom `paint()` widgets) on an
ESP32 with an SPI LCD, using the same `avnd::soft_ui::surface` that backs the
plug-in editors and the golden tests: push touch events in, pull an RGBA
framebuffer out, flush it to the panel.

**Status: illustrative, not CI-built.** The `surface` core is golden-tested
on desktop; this project is the platform glue and has not been run on
hardware yet. Expect to adapt the panel/touch setup to your board.

## Layout

- `main/avnd_ui_main.cpp` — panel init (`esp_lcd`), the UI loop, RGBA→RGB565
  conversion, and where to hook a touch controller.
- The plug-in is defined inline (a 320×240 gain UI); swap in any avendish
  processor with a `ui`.

## Memory

- The soft runtime rasterizes through canvas_ity, whose internal buffer is
  float RGBA (16 B/px): a 320×240 UI needs ~1.2 MB → **PSRAM required** with
  this painter. The planned ctx-based painter (RGB565-native, banded) is the
  path to SRAM-only boards; the `avnd::painter` concept makes that a swap.
- The RGBA8 framebuffer (300 KB) and RGB565 flush buffer (150 KB) are
  allocated from PSRAM below.

## Fonts

No filesystem is assumed: embed a TTF via `EMBED_FILES` (see
`main/CMakeLists.txt`) and register it with the font registry at startup.
Nuklear text measurement and canvas_ity rasterization share that font.

## Build

```sh
idf.py set-target esp32s3
idf.py build flash monitor
```

The avendish include directory and its fetched dependencies (nuklear,
canvas_ity) must be on the include path; see `main/CMakeLists.txt`.
