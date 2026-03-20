# Implementing a new concept

Let's say one wants to implement a new concept / feature that will be 
recognized by the Avendish bindings.

There are a few simple steps which separate concerns.

## 1. Making an example

The first step is to prototype the expected API through an example.
This helps making sure the idea is sound and useable.

For instance, let's say we want to add support for ports with a member variable `int color;` which would enable 
ports to have an associated color.

The color could be depending on the object a compile-time constant or a run-time property.

We will write a simple example object: 
```cpp
struct MyProcessor
{
  static consteval auto name() { return "Foo"; }

  struct
  {
    // A port with a color, "a";
    struct { 
      int color = 0xFF0000;
      // could also be int color() { return 0xFF0000; } 
      // or static constexpr int color = 0xFF0000;
      // etc...
      float value; 
    } a;
    
    // A port without a color, "b";
    struct { float value; } b; 
  } inputs;

  void operator()() { 
  }
};
```

## 2. Defining the concept

For instance, in `<avnd/concepts/color.hpp>`: 
```cpp
template<typename T> 
concept has_color = 
   requires { T::color; } 
|| requires (T t) { t.color; }
// etc as needed.
;

```

## 3. Defining getter functions to access it

For instance, in `<avnd/introspection/color.hpp>`: 
```cpp
template <avnd::has_color T>
consteval auto get_color()
{
  if constexpr(requires { T::color(); })
    return T::color();
  else if constexpr(requires { sizeof(decltype(T::color)); })
    return T::color;
  else
    // Anything that may create a compile error
    // for instance static_assert(std::is_void_v<T>);
    return T::there_is_no_color_here; 
}

template <typename T>
consteval auto get_color(const T& t)
{
  if constexpr(requires { int(t.color); })
    return t.color;
  else
    return get_color<T>();
}
```

## 4. Defining predicates to access ports that specifically have colors

For instance, in `<avnd/introspection/port.hpp>`

```cpp
template <typename Field>
using is_parameter_with_color_t = boost::mp11::mp_bool<has_color<Field> && parameter<Field>>;
template <typename T>
using parameter_with_color_introspection = predicate_introspection<T, is_parameter_with_color_t>;
```

And then some useful shortcuts for quickly sorting input: and output ports: 

For instance, in `<avnd/introspection/input.hpp>`

```cpp
template <typename T>
struct parameter_with_color_input_introspection
    : parameter_with_color_introspection<typename inputs_type<T>::type>
{
};
```

Now we can access all the ports that have a `color` member this way: 

```cpp
MyProcessor obj;
avnd::parameter_with_color_input_introspection<MyProcessor>::for_all(
    obj.inputs
  , [] (auto& field) { 
    // This function will only be called for input fields of `MyProcessor` which 
    // satisfy avnd::has_color
});
```

## 5. In bindings, storing some ancillary data for each field with a `color`

This is one of the most powerful zero-cost abstractions provided by Avendish:
it is possible to create arrays of data that specifically match the ports with specific properties, 
attached with the binding, in a way that an object without any ports using the feature won't bear a single byte of wasted 
space or instruction generated caused by the implementation of said feature.

For this, let's define an ancillary state that we want to store for each port that have a color.

```cpp
template <typename Field>
struct color_state_type;

template <avnd::has_color Field>
struct color_state_type<Field>
{
  int count = 0;
};
```

Then a storage class for the feature:

```cpp
// If no ports have a color, the storage will be an empty class.
template <typename T>
struct ports_with_color_storage
{
};

// Otherwise...
template <typename T>
  requires(avnd::parameter_with_color_input_introspection<T>::size > 0)
struct ports_with_color<T>
{
  using in_tuple = avnd::filter_and_apply<
    color_state_type, avnd::parameter_with_color_input_introspection, T>;

  [[no_unique_address]] in_tuple in_handles;
  
  // Here, in_tuple == std::tuple<color_state_type<decltype(a)>>;
  // b is completely ignored as it does not have a color.
};
```

Finally, in the binding class: 

```cpp
[[no_unique_address]] ports_with_color_storage<T> colors;
```


