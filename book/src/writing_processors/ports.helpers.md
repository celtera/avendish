# Ports with the helper library

Here is how our processor looks with the current set of helpers:

```cpp
#pragma once
#include <halp/controls.hpp>

struct MyProcessor
{
  // halp_meta(A, B) expands to static consteval auto A() { return B; }
  halp_meta(name, "Addition")

  struct
  {
    // val_port is a simple type which contains 
    // - a member value of type float
    // - the name() metadata method
    // - helper operators to allow easy assignment and use of the value.
    halp::val_port<"a", float> a;
    halp::val_port<"b", float> b;
  } inputs;

  struct
  {
    halp::val_port<"out", float> out;
  } outputs;

  void operator()() { outputs.out = inputs.a + inputs.b; }
};
```

If one really did not like templates, the following macro could be defined to make custom ports: 

```cpp
#define my_value_port(Name, Type)                  \
  struct {                                         \
    static consteval auto name() { return #Name; } \
    Type value;                                    \
  } Name;

// Used like:
my_value_port(a, float)
my_value_port(b, std::string)
... etc ...
```

Likewise if one day the [metaclasses](https://github.com/cplusplus/papers/issues/403) proposal comes to pass, it will be possible to convert:

```cpp
meta_struct
{
  float a;
  float b;
} inputs;
```

into a struct of the right shape, automatically, at compile-time, and all the current bindings will keep working.