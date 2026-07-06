# Avendish soft UI on a Soldered Inkplate 2 (3-color e-paper)

Renders a plug-in's `struct ui` — declarative layout plus a custom
`paint()` widget — headlessly with the soft runtime, quantizes the RGBA
frame to black/white/red, and pushes it to the Inkplate 2's 212×104
e-paper. A 3-color panel refreshes in ~15 s, so this is the *render-once*
flavour of the plan's §6.3 windowless shell; the instrument theme's orange
value accents come out as red ink.

## Requirements

- PlatformIO. The stock `espressif32` platform ships GCC 8 which cannot
  compile avendish (C++23); this project uses the
  [pioarduino](https://github.com/pioarduino/platform-espressif32) platform
  (Arduino core 3.x, GCC 13).
- Environment variables pointing at the headers (all header-only):
  `AVND_INCLUDE`, `AVND_NUKLEAR`, `AVND_CANVAS_ITY`, `BOOST_ROOT`,
  `MAGIC_ENUM` — a desktop avendish build's `_deps/` directory provides the
  fetched ones (see platformio.ini).

## Build & flash

```sh
pio run              # build
pio run -t upload    # flash (CH340 serial)
pio device monitor   # watch the render/refresh log
```

## Memory

The board's WROVER module has 8 MB PSRAM; `BOARD_HAS_PSRAM` routes the big
allocations (canvas_ity's float-RGBA internals, ~350 KB at 212×104, plus
the framebuffer and the embedded TTF) there automatically.
