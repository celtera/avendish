# Supported port types

The supported port types depend on the back-end. There is, however, some flexibility.

## Simple ports

### Float

They should work everywhere.

```cpp
struct {
  float value;
} my_port;
```

### Double

They will not work in Max / Pd message processors (but will work in Max / Pd audio processors)
as their API expect a pointer to an existing `float` value.

```cpp
struct {
  double value;
} my_port;
```

### Int

Same than double.

```cpp
struct {
  int value;
} my_port;
```

### Bool

Same than double.

```cpp
struct {
  bool value;
} my_port;
```

> Note that depending on the widget you use, UIs may create a `toggle`, 
> a maintained `button` or a momentary `bang`.

### String

Will not work in environments such as VST3 for obvious reasons.

```cpp
struct {
  std::string value;
} my_port;
```

## Enumerations

Enumerations are interesting.
There are multiple ways to implement them.

### Mapping a string to a value

Consider the following port:

```cpp
template<typename T>
using my_pair = std::pair<std::string_view, T>;
struct {
  halp_meta(name, "Enum 1");
  enum widget { combobox };

  struct range {
    my_pair<float> values[3]{{"Foo", -10.f}, {"Bar", 5.f}, {"Baz", 10.f}};
    int init{1}; // == Bar
  };

  float value{}; // Will be initialized to 5.f
} combobox;
```

Here, using a range definition of the form:

```cpp
struct range {
    <string -> value map> values[N] = { 
      { key_1, value_1}, { key_2, value_2 }, ... 
    };

    <integer> init = /* initial index */;
};
```

allows to initialize a combobox in the UI, with a predetermined set of values.
The value type is the actual one which will be used for the port - Avendish will translate 
as needed.

### Enumerating with only string

Consider the following port:

```cpp
struct {
  halp_meta(name, "Enum 2");
  enum widget { enumeration };

  struct range {
    std::string_view values[4]{"Roses", "Red", "Violets", "Blue"};
    int init{1}; // Red
  };

  std::string_view value;
};
```

Here, we can use `std::string_view`: the assigned value will always be 
one from the range::values array ; these strings live in static memory 
so there is no need to duplicate them in an `std::string`.

It is also possible to use an `int` for the port value:

```cpp
struct {
  halp_meta(name, "Enum 3");
  enum widget { enumeration };

  struct range {
    std::string_view values[4]{"Roses", "Red", "Violets", "Blue"};
    int init{1}; // Red
  };

  int value{};
};
```

Here, the int will just be the index of the selected thing.

### Enumerating with proper enums :-)

Finally, we can also use actual enums.

```cpp
enum my_enum { A, B, C };
struct {
  halp_meta(name, "Enum 3");
  enum widget { enumeration };

  struct range
  {
    std::string_view values[3]{"A", "B", "C"};
    my_enum init = my_enum::B;
  };

  my_enum value{};
}
```

> The enum must be contiguous, representable in an int32 and start at 0: 
> `enum { A = 3, B, C };` will not work. 
> `enum { A, B, C, AA = 10 };` will not work.
> `enum { A, B, C, ... 4 billion values later ..., XXXX };` will not work.
> `enum { A, B, C };` will work.

An helper is provided, which is sadly a macro as we cannot do proper enum reflection yet:

```cpp
halp__enum("Simple Enum", Peg, Square, Peg, Round, Hole) my_port;
```

declares a port named "Simple Enum". The default value will be "Peg", the 4 enumerators are Square, Peg, Round, Hole.


## Advanced types

### 2D position: xy

Here a special shape of struct is recognized:

```cpp
struct {
  struct { float x, y; } value;
} my_port;
```

### Color

Here a special shape of struct is recognized:

```cpp
struct {
  struct { float r, g, b, a; } value;
} my_port;
```
