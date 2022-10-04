# Layout-based UIs

> Supported bindings: ossia, others are work-in-progress

To define a custom UI, one has to add a `struct ui` in the processor definition.

```cpp
struct MyProcessor {
  struct ui {
   
  };
};
```

By default, this will do nothing: we have to fill it. `ui` will be the top-level widget.
Child widgets can be added simply by defining struct members.
Containers are defined by adding a `layout()` function which returns an enum value, which may 
currently be any of the following names:

```
hbox,
vbox,
container,
group,
split,
tabs,
grid,
spacing,
control,
custom
```

For instance:
```cpp
struct ui
{
  static constexpr auto layout() { enum { hbox } d{}; return d; }
  struct {
    static constexpr auto layout() { enum { vbox } d{}; return d; }
    const char* text = "text";
    decltype(&ins::int_ctl) int_widget = &ins::int_ctl;
  } widgets;

  struct {
    static constexpr auto layout() { enum { spacing } d{}; return d; }
    static constexpr auto width() { return 20; }
    static constexpr auto height() { return 20; }
  } a_spacing;

  const char* text = "text2";
};
```

This defines, conceptually, the following layout:

```ascii
|-----------------------------|
|  |  text  |                 |
|  |        |                 |
|  | =widg= |  <20px>  text2  |
|  |        |                 |
|  |        |                 |
|-----------------------------|
```

## Layouts

### HBox, VBox

These will layout things either horizontally or vertically.

### Split

Each children will be separated by a split line (thus generally one would use it to separate layouts).

### Grid

This will layout children items in a grid.

Either of `rows()` and `columns()` properties can be defined, but not both:
```
static constexpr auto rows() { return 3; }
static constexpr auto columns() { return 3; }
```

If `columns()` is defined, children widget will be laid out in the first row until the column count is reached, then in the second row, etc. until there are no more children items, and conversely if `rows()` is defined.

That is, given: 
```cpp
struct {
  static constexpr auto layout() { enum { grid } d{}; return d; }
  static constexpr auto columns() { return 3; }
  const char* text1 = "A";
  const char* text2 = "B";
  const char* text3 = "C";
  const char* text4 = "D";
  const char* text5 = "E";
} a_grid;
```

The layout will be:
```ascii
|---------|
| A  B  C | 
| D  E    |
|---------|
```

Instead, if `rows()` is defined to 3:

```ascii
|------|
| A  D | 
| B  E | 
| C    |
|------|
```

### Tabs

Tabs will display children items in tabs.
Each children item should have a `name()` property which will be shown in the tab bar.

```cpp
struct {
  static constexpr auto layout() { enum { tabs } d{}; return d; }
  struct {
    static constexpr auto layout() { enum { hbox } d{}; return d; }
    static constexpr auto name() { return "First tab"; }
    const char* text1 = "A";
    const char* text2 = "B";
  } a_hbox;
  struct {
    static constexpr auto layout() { enum { hbox } d{}; return d; }
    static constexpr auto name() { return "Second tab"; }
    const char* text3 = "C";
    const char* text4 = "D";
  } a_vbox;
} a_tabs;
```

## Properties

### Background color
Background colors can be chosen from a standardized set: for now, those are fairly abstract to allow things to work in a variety of environments.

```
darker,
dark,
mid,
light,
lighter
```

Setting the color is done by adding this to a layout:

```cpp
static constexpr auto background() { enum { dark } d{}; return d; }
```

### Explicit positioning

In "group" or "container" layouts, widgets will not be positioned automatically. `x` and `y` methods can be used for that.

```cpp
static constexpr auto x() { return 20; }
static constexpr auto y() { return 20; }
```

### Explicit sizing

Containers can be given an explicit (device independent) pixel size with 

```cpp
static constexpr auto width() { return 100; }
static constexpr auto height() { return 50; }
```

Otherwise, things will be made to fit in a best-effort way.

## Items

## Text labels
The simplest item is the text label: simply adding a `const char*` member is sufficient. 

## Controls
One can add a control (either input or output) simply by adding a member pointer to it: 
```cpp
struct MyProcessor {
  struct ins {
    halp::hslider_f32<"Foo"> foo;
  } inputs;

  struct ui
  {
    static constexpr auto layout() { enum { hbox } d{}; return d; }
    const char* text = "text";
    decltype(&ins::foo) int_widget = &ins::foo;
  };
};
```

The syntax without helpers currently needs some repeating as C++ does not yet allow `auto` as member fields, otherwise it'd just be:

```cpp
auto int_widget = &ins::foo;
```

# Helpers

Helpers simplify common tasks ; here, C++20 designated-initializers allow us to have a very pretty API and reduce repetitions:

## Widget helpers
```cpp
halp::label l1{
    .text = "some long foo"
  , .x = 100
  , .y = 25
};

halp::item<&ins::foo> widget{
    .x = 75
  , .y = 50
};
```

## Properties helpers

```cpp
struct ui {
 // If your compiler is recent enough you can do this, 
 // otherwise layout and background enums have to be qualified:
 using enum halp::colors;
 using enum halp::layouts;
 
 halp_meta(name, "Main")
 halp_meta(layout, hbox)
 halp_meta(background, mid)

 struct {
   halp_meta(name, "Widget")
   halp_meta(layout, vbox)
   halp_meta(background, dark)
   halp::item<&ins::int_ctl> widget;
   halp::item<&outs::measure> widget2;
 } widgets;
};
```

