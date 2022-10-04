# Port update callback

> Supported bindings: ossia

It is possible to get a callback whenever the value of a (value) input port gets updated, to perform complex actions.
`update` will always be called before the current tick starts.

The port simply needs to have a `void update(T&) { }` method implemented, where `T` will be the object containing the port:

## Example 
```cpp
struct MyProcessor
{
  static consteval auto name() { return "Addition"; }

  struct
  {
    struct { 
      static consteval auto name() { return "a"; } 
      void update(MyProcessor& proc) { /* called when 'a.value' changes */ }

      float value; 
    } a;
    struct { 
      static consteval auto name() { return "b"; } 
      void update(MyProcessor& proc) { /* called when 'b.value' changes */ }

      float value; 
    } b;
  } inputs;

  struct
  {
    struct { 
      static consteval auto name() { return "out"; } 
      float value; 
    } out;
  } outputs;

  void operator()() { outputs.out.value = inputs.a.value + inputs.b.value; }
};
```

## Usage with helpers

To add an update method to an helper, simply inherit from them: 


```cpp
#pragma once
#include <halp/controls.hpp>

struct MyProcessor
{
  halp_meta(name, "Addition")

  struct
  {
    struct : halp::val_port<"a", float> { 
      void update(MyProcessor& proc) { /* called when 'a.value' changes */ }
    } a;

    halp::val_port<"b", float> b;
  } inputs;

  struct
  {
    halp::val_port<"out", float> out;
  } outputs;

  void operator()() { outputs.out = inputs.a + inputs.b; }
};
```