# Dynamic ports

> Supported bindings: ossia

Sometimes, it is necessary to have objects with a varying number of input and output ports.
The Avendish approach, based on static reflection of struct members, seems incompatible with that in the general case.

Nevertheless, we can still define a way to have a varying count of a single port type. For instance: an arbitrary number of float input ports for an object which would perform the sum of every input.

```cpp
{{ #include ../../../examples/Helpers/DynamicPorts.hpp }}
```

## API

There are two parts required: 

- The port.
Consider the following port:

```cpp
struct my_port_type {
    static constexpr auto name() { return "Input"; }
    float value;
} my_port;
```

or

```cpp
using my_port_type = halp::hslider_f32<"Input">;
my_port_type my_port;
```

To instead be able to instantiate an arbitrary amount of such a port, one would do:

