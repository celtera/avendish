# Getting started

Here is a minimal, self-contained definition of an Avendish processor:

```cpp
import std;

[[name: "Hello World"]]
export struct MyProcessor
{
  void operator()() { 
    std::print("Henlo\n");
  }
};
```

... at least, in an alternative universe where C++ has gotten custom attributes and reflection on those, 
and where modules and `std::print` work consistently across all compilers ; in our universe, this is still a few years away. Keep hope, dear reader, keep hope !

# Getting started, for good

Here is a minimal, self-contained definition of an Avendish processor, which works on 2022 compilers:

```cpp
#pragma once
#include <cstdio>

struct MyProcessor
{
  static consteval auto name() { return "Hello World"; }

  void operator()() { 
    printf("Henlo\n");
  }
};
```

Yes, it's not much. You may even already have some in your codebase without even being aware of it ! 

Now, you may be used to the usual APIs for making audio plug-ins and start wondering about all the things you are used too and that are missing here: 

- Inheritance or shelving function pointers in a C struct.
- Libraries: defining an Avendish processor does not in itself require including anything. 
  A central point of the system is that everything can be defined through bare C++ constructs, without requiring the user to import types from a library. A library of helpers is nonetheless provided, to simplify some repetitive cases, but is in no way mandatory ; if anything, I encourage anyone to try to make different helper APIs that fit different coding styles.
- Functions to process audio such as
```cpp  
void process(double** inputs, double** outpus, int frames);
```

We'll see how all the usual amenities can be built on top of this and simple C++ constructs such as variables, methods and structures.

## Line by line

```cpp
// This line is used to instruct the compiler to not include a header file multiple times.
#pragma once

// This line is used to allow our program to use `printf`:
#include <cstdio>

// This line declares a struct named HelloWorld. A struct can contain functions, variables, etc.
// It could also be a class - in C++, there is no strong semantic difference between either.
struct MyProcessor
{
  // This line declares a function that will return a visible name to show to our 
  // users.
  // - static is used so that an instance of HelloWorld is not needed: 
  //   we can just refer to the function as HelloWorld::name();
  // - consteval is used to enforce that the function can be called at compile-time, 
  //   which may enable optimizations in the backends that will generate plug-ins.
  // - auto because it does not matter much here, we know that this is a string :-)
  static consteval auto name() { return "Hello World"; }

  // This line declares a special function that will allow our processor to be executed as follows: 
  // 
  // HelloWorld the_processor;
  // the_processor();
  //
  // ^ the second line will call the "operator()" function.
  void operator()() 
  { 
    // This one should hopefully be obvious :-)
    printf("Henlo\n");
  }
};
```
