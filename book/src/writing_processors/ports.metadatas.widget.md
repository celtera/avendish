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
hbargraph, vbargraph, bargraph,
range
```

This kind of widget definition is here to enable host DAWs to automatically generate appropriate UIs automatically.

A further chapter will present how to create entirely custom painted UIs and widgets.