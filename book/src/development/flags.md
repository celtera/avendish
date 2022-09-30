# Flags

Flags are a very simple concept in Avendish to declare that a given struct (most of the time a port) has some specific feature.

It just means that a specific keyword has to be in some way visible inside the struct.

The simplest way is through an anonymous enumeration:

```cpp
struct my_port
{
  // Defines the bamboozle flag
  enum { bamboozle };

  // Other ways to define flags, A, B, C, D, E, F....:
  enum A { };
  struct B;
  using C = void;
  int D; // Really wasteful though!
  static constexpr bool E = 0;
  void F() { }
};
```

From there, the binding code can check for the flag at compile-time:

```cpp
template<typename T> 
concept has_bamboozle_flag = 
   requires { T::bamboozle; } 
|| requires { sizeof(typename T::bamboozle*); }
;
```

The `halp_flag()` and `halp_flags()` macro wrap the basic enum definition:

```cpp
struct my_port
{
  halp_flag(bamboozle_flag);
  halp_flags(foo, bar);
};
```