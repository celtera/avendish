# Defining UI widgets

> Supported bindings: ossia

Avendish can recognize a few names that will indicate that a widget of a certain type must be created.

For instance: 

```cpp
struct {
  enum { knob };
  static consteval auto name() { return "foobar"; } 

  struct range {
    float min = -1.;
    float max = 1.;
    float init = 0.5;
  };

  float value{};
} foobar;
```

Simply adding the enum definition in the struct will allow the bindings to detect it at compile-time, and instantiate an appropriate UI control.

The following widget names are currently recognized: 

```
bang, impulse
button, pushbutton
toggle, checkbox,
hslider, vslider, slider
spinbox,
knob,
lineedit,
choices, enumeration
combobox, list
xy, xy_spinbox,
xyz, xyz_spinbox,
color, 
time_chooser,
grid_chooser,
hbargraph, vbargraph, bargraph,
range_slider, range_spinbox
```

This kind of widget definition is here to enable host DAWs to automatically generate appropriate UIs automatically.

## Time and grid choosers

`halp::time_chooser` is a duration: a knob whose readout switches between free
seconds and a note value synced to the tempo. The object only sees seconds (and
a `sync` flag): the host recomputes them when the tempo changes.

```cpp
halp::time_chooser<"Delay", halp::range{.min = 0.001, .max = 5., .init = 0.25}> delay;
```

`halp::grid_chooser` (`<halp/grid_chooser.hpp>`) is a quantization grid. Its
value is musical and the object receives it as is, a `halp::grid_value`:

```cpp
struct grid_value
{
  grid_unit unit;  // free, note, bars, period (and parent, reserved)
  double value;
  double offset;   // reserved
};
```

| unit     | value                                   | e.g.                     |
|----------|-----------------------------------------|--------------------------|
| `free`   | seconds; 0 means no quantization        | `0.5 s`, `None`          |
| `note`   | fraction of a whole note (4 quarters)   | `1/8T` is 1/12, `1/8.` is 3/16, `2/1` is 2 |
| `bars`   | bars of the current signature           | `1 bar`, `3 bars`        |
| `period` | multiple of the object's own period     | `Loop`, `2x loop`, `1/2 loop` |

A note value is always that many whole notes, never a bar: `1/1` is four
quarters in 3/4 too. The object declares which units the user may pick, the
default, the longest grid in bars and the name of its period:

```cpp
halp::grid_chooser<
    "Quantize",
    halp::grid_setup{
        .units = halp::grid_units::musical | halp::grid_units::period,
        .init = halp::grid_value::bars(1),
        .max_bars = 8},
    "loop">
    quantize;
```

`halp::next_grid_point` (`<halp/grid.hpp>`) finds where the next point of a
grid falls in the current buffer, from a `halp::tick_musical` and, for the
`period` unit, the object's period length and position in frames. It does not
allocate and can be called from the audio thread:

```cpp
void operator()(halp::tick_musical tk)
{
  const halp::grid_period loop{.length = loop_frames, .position = played_frames};
  if(auto frame = halp::next_grid_point(inputs.quantize.value, tk, sample_rate, loop))
    start_at(*frame);
}
```

Notes shorter than a bar restart at every bar line; bars and longer notes
count from the last signature change; free times and periods count from the
period's origin (or the start of playback, for free times without a period).

A further chapter will present how to create entirely custom painted UIs and widgets.