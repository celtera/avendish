# Initialization

> Supported bindings: Max, Pd

Some media systems provide a way for objects to be passed initialization arguments.

Avendish supports this with a special "initialize" method. Ultimately, I'd like to be able to simply use C++ constructors for this, but haven't managed to yet.

Here's an example: 

```cpp
struct MyProcessor {
void initialize(float a, std::string_view b)
{
  std::cout << a << " ; " << b << std::endl;
}
...
};
```

Max and Pd will report an error if the object is not initialized correctly, e.g. like this: 

    [my_processor 1.0 foo]  // OK
    [my_processor foo 1.0]  // Not OK
    [my_processor] // Not OK
    [my_processor 0 0 0 1 2 3] // Not OK
