# Callbacks

> Supported bindings: ossia, Max, Pd, Python

Just like messages allow to define functions that will be called from an outside request, it is also possible to define callbacks: functions that our processor will call, and which will be sent to the outside world.

Just like for messages, this does not really make sense for instance for audio processors ; however it is pretty much necessary to make useful Max or Pd objects.

Callbacks are defined as part of the `outputs` struct.

## Defining a callback with std::function

This is a first possibility, which is pretty simple:

```cpp
struct {
  static consteval auto name() { return "bong"; }
  std::function<void(float)> call;
}; 
```

The bindings will make sure that a function is present in `call`, so that our code can call it: 

```cpp
struct MyProcessor {
  static consteval auto name() { return "Distortion"; }

  struct {
    struct {
      static consteval auto name() { return "overload"; }
      std::function<void(float)> call;
    } overload; 
  } outputs;

  float operator()(float input) 
  {
    if(input > 1.0)
      outputs.overload.call(input);

    return std::tanh(input); 
  }
};
```

However, we also want to be able to live without std:: types ; in particular, std::function is a quite complex beast which does type-erasure, potential dynamic memory allocations, and may not be available on all platforms.

Thus, it is also possible to define callbacks with a simple pair of function-pointer & context: 

```cpp
struct {
  static consteval auto name() { return "overload"; }
  struct {       
    void (*function)(void*, float);
    void* context;
  } call;
} overload;
```

The bindings will fill the function and function pointer, so that one can call them: 

```cpp
float operator()(float input) 
{
  if(input > 1.0)
  {
    auto& call = outputs.overload.call;
    call.function(call.context, input);
  }
  return std::tanh(input); 
}
```

Of course, this is fairly verbose: thankfully, helpers are provided to make this as simple as `std::function` but without the overhead (until `std::function_view` gets implemented):

```cpp
struct {
  static consteval auto name() { return "overload"; }
  halp::basic_callback<void(float)> call;
} overload;
```