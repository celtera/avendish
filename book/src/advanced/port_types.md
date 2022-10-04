# Supported port types

The supported port types depend on the back-end. There is, however, some flexibility.

## Simple ports

### Float

> Supported bindings: all

```cpp
struct {
  float value;
} my_port;
```

### Double

> Supported bindings: all except Max / Pd message processors (they will work in Max / Pd audio processors) as their API expect a pointer to an existing `float` value.

```cpp
struct {
  double value;
} my_port;
```

### Int

> Supported bindings: Same than double.

```cpp
struct {
  int value;
} my_port;
```

### Bool

> Supported bindings: Same than double.

```cpp
struct {
  bool value;
} my_port;
```

> Note that depending on the widget you use, UIs may create a `toggle`,
> a maintained `button` or a momentary `bang`.

### String

> Supported bindings: ossia, Max, Pd, Python

```cpp
struct {
  std::string value;
} my_port;
```

## Enumerations

> Supported bindings: all

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

## Containers

> Supported bindings: ossia

Containers are supported (in environments where this is meaningful) provided that they provide an API that matches:

### `std::vector`

```cpp
template <typename T>
concept vector_ish = requires(T t)
{
  t.push_back({});
  t.size();
  t.reserve(1);
  t.resize(1);
  t.clear();
  t[1];
};
```

For instance, `boost::static_vector`, `boost::small_vector` or `absl::InlinedVector` all satisfy this and can be used as a value type.

```cpp
struct {
  std::vector<float> value;
} my_port;
```

### `std::set`

For instance, `std::set`, `std::unordered_set` or `boost::container::flat_set` can be used as a value type.

```cpp
struct {
  boost::container::flat_set<float> value;
} my_port;
```

### `std::map`

For instance, `std::map`, `std::unordered_map` or `boost::container::flat_map` can be used as a value type.

```cpp
struct {
  boost::container::flat_map<float> value;
} my_port;
```

### C arrays

C arrays aren't supported due to limitations in the reflection capabilities of C++: 

```cpp
struct {
  int value[2]; // Won't work
} my_port;
```

or

```cpp
struct {
  struct { int v[2]; } value; // Won't work
} my_port;
```

Use `std::array` instead.

## Variants

> Supported bindings: ossia

Types which look like `std::variant` (for instance `boost::variant2::variant` or `mpark::variant`) are supported.

```cpp
struct {
  std::variant<int, bool, std::string> value;
} my_port;
```

## Optionals

> Supported bindings: ossia

Types which look like `std::optional` (for instance `boost::optional` or `tl::optional`) are supported.

```cpp
struct {
  std::optional<int> value;
} my_port;
```

This is used to give message semantics to the port: optionals are reset before execution of the current 
tick, both for inputs and outputs. If an input is set, it means that a message was received for this tick.
If the processor sets the output, a message will be sent outwards.

This is mostly equivalent to messages and callbacks, but with a value-based instead of function-based API
(and thus a small additional storage cost).

## Advanced types

### 2D position: xy

> Supported bindings: ossia

Here a special shape of struct is recognized:

```cpp
struct {
  struct { float x, y; } value;
} my_port;
```

### Color

> Supported bindings: ossia

Here a special shape of struct is recognized:

```cpp
struct {
  struct { float r, g, b, a; } value;
} my_port;
```

### Generalized aggregate types

> Supported bindings: ossia

Aggregates are *somewhat* supported: that is, one can define 

```cpp
struct Foo {
  int a, b;
  struct {
    std::vector<float> c; 
    std::string d;
  };
  std::array<bool, 4>;
};
```

and use this as a value type in a port.
This is so far only supported in ossia, and will not preserve names, but be translated as:

```cpp
(list) [
  (int)a
, (int)b
, (list) [ 
    (list)[ c0, c1, c2, ... ]
  , (string)"d"
  ]
]
```
