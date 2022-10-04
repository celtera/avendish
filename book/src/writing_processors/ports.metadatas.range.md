# Defining a min/max range

> Supported bindings: all

Here is how one can define a port with such a range:

```cpp
struct {
  static consteval auto name() { return "foobar"; } 

  struct range {
    float min = -1.;
    float max = 1.;
    float init = 0.5;
  };

  float value{};
} foobar;
```

Here is another version which will be picked up too: 
```cpp
struct {
  static consteval auto name() { return "foobar"; } 
  static consteval auto range() {
    struct { 
      float min = -1.;
      float max = 1.;
      float init = 0.5;
    } r;
    return r;
  };

  float value{};
} foobar;
```

More generally, in most cases, Avendish will try to make sense of the things the author declares, whether they are types, variables or functions. This is not implemented entirely consistently yet, but it is a goal of the library in order to enable various coding styles and as much freedom of expression as possible for the media processor developer.

## Keeping metadata static
Note that we should still be careful in our struct definitions to not declare normal member variables for common metadata, which would take valuable memory and mess with our cache lines. This reduces performance for no good reason: imagine instantiating 10000 "processor" objects, you do not want each processor to carry the overhead of storing the range as a member variable, such as this: 

```cpp
struct {
  const char* name = "foobar";

  struct {
    float min = -1.;
    float max = 1.;
    float init = 0.5;
  } range;

  float value{};
} foobar;

// In this case:
static_assert(sizeof(foobar) == 4 * sizeof(float) + sizeof(const char*));
// sizeof(foobar) == 24 on 64-bit systems

// While in the previous cases, the "name" and "range" information is stored in a static space in the binary ; its cost is paid only once:
static_assert(sizeof(foobar) == sizeof(float));
// sizeof(foobar) == 4
```

## Testing on a processor
If we modify our example processor this way: 

```cpp
struct MyProcessor
{
  static consteval auto name() { return "Addition"; }

  struct
  {
    struct { 
      static consteval auto name() { return "a"; } 
      struct range {
        float min = -10.;
        float max = 10.;
        float init = 0.;
      };
      float value; 
    } a;
    struct { 
      static consteval auto name() { return "b"; } 
      struct range {
        float min = -1.;
        float max = 1.;
        float init = 0.;
      };
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

then some backends will start to be able to do interesting things, like showing relevant UI widgets, or clamping the inputs / outputs.

This is not possible in all back-ends, sadly. Consider for instance PureData: the way one adds a port is by passing a pointer to a floating-point value to Pd, which will write directly the inbound value at the memory address: there is no point at which we could plug-in to perform clamping of the value. 

Two alternatives would be possible in this case: 
- Change the back-end to instead expect all messages on the first inlet, as those can be captured. This would certainly yield lower performance as one now would have to pass a symbol indicating the parameter so that the object knows to which port the input should map.
- Implement an abstraction layer which would duplicate the parameters with their clamped version, and perform the clamping on all parameters whenever the process function gets called. This would however be hurtful in terms of performance and memory use.
 