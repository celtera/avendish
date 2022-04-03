# Layout-based UIs

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

# Properties

## Background color
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

## Explicit positioning

In "group" or "container" layouts, widgets will not be positioned automatically. `x` and `y` methods can be used for that.

```cpp
static constexpr auto x() { return 20; }
static constexpr auto y() { return 20; }
```

## Explicit sizing

Containers can be given an explicit (device independent) pixel size with 

```cpp
static constexpr auto width() { return 100; }
static constexpr auto height() { return 50; }
```

Otherwise, things will be made to fit in a best-effort way.

# Items

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

Helpers simplify common tasks ; here, C++20 designated-initializers allow us to have a very pretty API:

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