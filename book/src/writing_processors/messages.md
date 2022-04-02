# Messages

So far, we already have something which allows to express a great deal of audio plug-ins, as well as many objects that do not operate in a manner synchronized to a constant sound input, but also in a more asynchronous way, and with things more complicated than single `float`, `int` or `string` values.

A snippet of code is worth ten thousand words: here is how one defines a message input.

```cpp
struct MyProcessor {
  struct messages {
    struct {
      static consteval auto name() { return "dump"; }
      void operator()(MyProcessor& p, double arg1, const std::string& arg2) {
        std::cout << arg1 << ";" << arg2 << "\n";
      }
    } my_message;
  };
};
```    

Messages are of course only meaningful in environments which support them. 
One argument messages are equivalent to arguments.
If there is more than one arguments, not all host systems may be able to handle them ; for instance, it does not make much sense for VST plug-ins. On the other hand, programming language bindings or systems such as Max and PureData have no problem with them.

## Passing existing functions

The following syntaxes are also possible:

```cpp
void free_function() { printf("Free function\n"); }

struct MyProcessor {
  void my_member(int x);

  struct messages {
    // Using a pointer-to-member function
    struct {
      static consteval auto name() { return "member"; }
      static consteval auto func() { return &MyProcessor::my_member; }
    } member;

    // Using a lambda-function
    struct
    {
      static consteval auto name() { return "lambda_function"; }
      static consteval auto func() {
        return [] { printf("lambda\n"); };
      }
    } lambda;

    // Using a free function
    struct
    {
      static consteval auto name() { return "function"; }
      static consteval auto func() { return free_function; }
    } freefunc;
  };
};
```

In every case, if one wants access to the processor object, it has to be the first argument of the function (except the non-static-member-function case where it is not necessary as the function already has access to the `this` pointer by definition).

## Type-checking
Messages are type-checked: in the example above, for instance, PureData will return an error for the message `[dump foo bar>`. For the message `[dump 0.1 bar>` things will however work out just fine :-)

## Arbitrary inputs
It may be necessary to have messages that accept an arbitrary number of inputs.
Here is how: 

```cpp
struct {
  static consteval auto name() { return "args"; }
  void operator()(MyProcessor& p, std::ranges::input_range auto range) {
    for(const std::variant& argument : range) {
      // Print the argument whatever the content
      // (a library such as fmt can do that directly)
      std::visit([](auto& e) { std::cout << e << "\n"; }, argument);

      // Try to do something useful with it - here the types depend on what the binding give us. So far only Max and Pd support that so the only possible types are floats, doubles and std::string_view
      if(std::get_if<double>(argument)) { ... }
      else if(std::get_if<std::string_view>(argument)) { ... }
      // ... etc
    }
  }
} my_variadic_message;
```

# Overloading

Overloading is not supported yet, but there are plans for it.

# How does the above code work ?

I think that this case is pretty nice and a good example of how C++ can greatly improve type safety over C APIs: a common problem for instance with Max or Pd is accessing the incorrect member of an union when iterating the arguments to a message.

Avendish has the following method, which transforms a Max or Pd argument list, into an iterable coroutine-based range of `std::variant`.

```cpp
using atom_iterator = avnd::generator<std::variant<double, std::string_view>>;
inline atom_iterator make_atom_iterator(int argc, t_atom* argv)
{
  for (int i = 0; i < argc; ++i) {
    switch (argv[i].a_type) {
      case A_FLOAT: {
        co_yield argv[i].a_w.w_float;
        break;
      }
      case A_SYM: {
        co_yield std::string_view{argv[i].a_w.w_sym->s_name};
        break;
      }
      default:
        break;
    }
  }
}
```

Here, `atom_iterator` is what gets passed to `my_variadic_message`. It allows to deport the iteration of the loop over the arguments into the calling code, but handles the matching from type to union member in a generic way, which removes an entire class of errors.
