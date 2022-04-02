# Refactoring

One can see how writing:

```cpp
struct { 
  static consteval auto name() { return "foobar"; } 
  float value; 
} foobar;
```

for 200 controls would get boring quick. In addition, the implementation of our processing function is not as clean as we'd want: in an ideal world, it would be just: 

```cpp
void operator()() { outputs.out = inputs.a + inputs.b; }
```

Thankfully, we can introduce our own custom abstractions without breaking anything: the only thing that matters is that they follow the "shape" of what a parameter is.

This shape is defined (as a first approximation) as follows:

```cpp
template<typename T>
concept parameter = requires (T t) { t.value = {}; };
```

In C++ parlance, this means that a type can be recognized as a parameter if

 - It has a member called `value`.
 - This member is assignable with some default value.

For instance: 

```cpp
struct bad_1 {
  const int value;
}; 

struct bad_2 {
  void value();
}; 

class bad_3 {
  int value;
}; 
```

are all invalid parameters.

This can be ensured easily by [asking the compiler](https://gcc.godbolt.org/z/c9Ko4ssM8): 
```
static_assert(!parameter<bad_1>);
static_assert(!parameter<bad_2>);
static_assert(!parameter<bad_3>);
```

Avendish will simply not recognize them and they won't be accessible anywhere.

Here are examples of valid parameters:
 
```cpp
struct good_1 {
  int value;
}; 

struct good_2 {
  std::string value;
}; 

template<typename T>
struct assignable {
  T& operator=(T x) { 
    printf("I changed !");
    this->v = x;
    return this->v;
  }
  T v;
};

class good_3 {
  public:
    assignable<double> value;
}; 
```

This can be ensured easily by [asking the compiler](https://gcc.godbolt.org/z/P7aET4q3z): 
```
static_assert(parameter<good_1>);
static_assert(parameter<good_2>);
static_assert(parameter<good_3>);
```

Avendish provides an helper library, `halp` (Helper Abstractions for Literate Programming), which match this pattern. However, users are encouraged to develop their own abstractions that fit their preferred coding style :-)
