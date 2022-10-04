# Widget helpers

> Supported bindings: depending on the underlying data type

To simplify the common use case of defining a port such as "slider with a range", a set of common helper types is provided.

Here is our example, now as refined as it can be ; almost no character is superfluous or needlessly repeated except the names of controls:

```cpp
#pragma once
#include <halp/controls.hpp>

struct MyProcessor
{
  halp_meta(name, "Addition")

  struct
  {
    halp::hslider_f32<"a", halp::range{.min = -10, .max = 10, .init = 0}> a;
    halp::knob_f32<"b" , halp::range{.min = -1, .max = 1, .init = 0}> b;
  } inputs;

  struct
  {
    halp::hbargraph_f32<"out", halp::range{.min = -11, .max = 11, .init = 0}> out;
  } outputs;

  void operator()() { outputs.out = inputs.a + inputs.b; }
};
```

This is how an environment such as *ossia score* renders it: 

![Addition](images/addition-score.gif)

Note that even with our helper types, the following holds:

```cpp
static_assert(sizeof(MyProcessor) == 3 * sizeof(float));
```

That is, an instance of our object weighs in memory exactly the size of its inputs and outputs, nothing else. In addition, the binding libraries try extremely hard to not allocate any memory dynamically, which leads to very concise memory representations of our media objects.