# Type predicates and introspection 

The core enabler of Avendish is the ability to filter struct definitions according to predicates.

That is, given: 

```cpp
struct Foo {
  int16_t x;
  float y;
  int32_t z;
  std::string txt;        
};
```

we want to filter for instance all integral types, and get:

```cpp
tuple<int16_t, int32_t> filtered = filter_integral_types(Foo{});
```

or run code on the members and other similar things.

## Fields introspection

The template `fields_introspection<T>` allows to introspect the fields of a struct in a generic way.

### Doing something for each member of a struct without an instance

```cpp
fields_instrospection<T>::for_all(
    [] <std::size_t Index, typename T> (avnd::field_reflection<Index, T>) {
      // Index is the index of the struct member, e.g. 1 for y in the above example
      // T is the type of the struct member
});
```

### Doing something for each member of a struct with an instance

```cpp
Foo foo;
fields_instrospection<T>::for_all(
    foo,
    [] <typename T> (T& t) {
      // t will be x, y, z, txt.
});
```

### Doing something for the Nth member of a struct without an instance

This function lifts a run-time index into a compile-time value. 
This is useful for instance for mapping a run-time parameter id coming from a DAW, 
into a field, such as when a parameter changes.

```cpp
fields_instrospection<T>::for_nth(
    index,
    [] <std::size_t Index, typename T> (avnd::field_reflection<Index, T>) {
      // Index is the index of the struct member, e.g. 1 for y in the above example
      // T is the type of the struct member
});
```

### Doing something for each member of a struct with an instance

Same as above but with an actual instance.

```cpp
Foo foo;
fields_instrospection<T>::for_nth(
    foo,
    index,
    [] <typename T> (T& t) {
      // t will be x, y, z, txt.
});
```

### Getting the index of a member pointer

```cpp
Foo foo;
avnd::index_in_struct(foo, &Foo::z) == 2;

```

## Predicate introspection

The type `predicate_introspection<T, Pred>` allows similar operations on a filtered subset of the struct members.

For instance, given the predicate: 

```cpp
template<typename Field>
using is_int<Field> = std::integral_constant<bool, std::is_integral_v<Field>>;
```

then 

```cpp
using ints = predicate_introspection<Foo, is_int>;
```

will allow to query all the `int16_t` and `int32_t` members of the struct. 
This example is assumed for all the cases below.

`predicate_introspection<Foo, is_int>` will work within this referential: indices will refer to 
these types. That is, the element 0 will be `int16_t` and element 1 will be `int32_t`. 
Multiple methods are provided to go from and to indices in the complete struct, to indices in the filtered version: 

|                |   |   |   |     | 
|----------------|---|---|---|-----| 
| member         | x | y | z | txt |
| field index    | 0 | 1 | 2 | 3   | 
| filtered index | 0 | - | 1 | -   | 


### Doing something for each member of a struct without an instance

```cpp
predicate_instrospection<T, P>::for_all(
    [] <std::size_t Index, typename T> (avnd::field_reflection<Index, T>) {
        // Called for x, z
        // Index is 0 for x, 1 for z
});
```

### Doing something for each member of a struct with an instance

```cpp
Foo foo;
predicate_instrospection<T, P>::for_all(
    foo,
    [] <typename T> (T& t) {
      // Called for x, z
});
```

```cpp
// This version also passes the compile-time index
Foo foo;
predicate_instrospection<T, P>::for_all_n(
    foo,
    [] <std::size_t Index, typename T> (T& t, avnd::predicate_index<Index>) {
      // x: Index == 0
      // y: Index == 1
});
```

```cpp
// This version passes both the field index and the filtered index
Foo foo;
predicate_instrospection<T, P>::for_all_n(
    foo,
    [] <std::size_t LocalIndex, std::size_t FieldIndex, typename T> (T& t, avnd::predicate_index<LocalIndex>, avnd::field_index<FieldIndex>) {
      // x: LocalIndex == 0 ; FieldIndex == 0
      // y: LocalIndex == 1 ; FieldIndex == 2
});
```

```cpp
// This version will return early if the passed lambda returns false
Foo foo;
bool ok = predicate_instrospection<T, P>::for_all_unless(
    foo,
    [] <typename T> (T& t) -> bool {
      return some_condition(t);
});
```
### Doing something for the Nth member of a struct without an instance

Two cases are possible depending on whether one has an index in the struct, 
or an index in the filtered part of it:

```cpp
predicate_instrospection<T, P>::for_nth_raw(
    field_index,
    [] <std::size_t Index, typename T> (avnd::field_reflection<Index, T>) {
       // field_index == 0: x
       // field_index == 1: nothing
       // field_index == 2: z
       // field_index == 3: nothing
});
```

```cpp
predicate_instrospection<T, P>::for_nth_mapped(
    filtered_index,
    [] <std::size_t Index, typename T> (avnd::field_reflection<Index, T>) {
       // filtered_index == 0: x
       // filtered_index == 1: z
});
```

### Doing something for each member of a struct with an instance

Same as above but with an actual instance.

```cpp
Foo foo;
predicate_instrospection<T, P>::for_nth_raw(
    foo,
    field_index,
    [] <std::size_t Index, typename T> (T& t) {
       // field_index == 0: x
       // field_index == 1: nothing
       // field_index == 2: z
       // field_index == 3: nothing
});
```

```cpp
Foo foo;
predicate_instrospection<T, P>::for_nth_mapped(
    foo,
    filtered_index,
    [] <std::size_t Index, typename T> (T& t) {
       // filtered_index == 0: x
       // filtered_index == 1: z
});
```

### Getting the type of the Nth element

```cpp
// int16_t
using A = typename predicate_instrospection<T, P>::nth_element<0>;
// int32_t
using B = typename predicate_instrospection<T, P>::nth_element<1>;
```

### Getting the Nth element

```cpp
Foo foo;

int16_t& a = predicate_instrospection<T, P>::get<0>(foo);
int32_t& b = predicate_instrospection<T, P>::get<1>(foo);
```

### Getting a tuple of the elements

```cpp
Foo foo;

// Get references:
std::tuple<int16_t&, int32_t&> tpl = predicate_instrospection<T, P>::tie(foo);

// Get copies:
std::tuple<int16_t, int32_t> tpl = predicate_instrospection<T, P>::make_tuple(foo);

// Apply a function to each and return the tuple of that
std::tuple<std::vector<int16_t>, std::vector<int32_t>> tpl = 
  predicate_instrospection<T, P>::filter_tuple(
    foo, 
    [] <typename T>(T& member) { return std::vector<T>{member}; });
```

### Getting the index of the filtered element

```cpp
// Go from index in the filtered members to index in the struct
predicate_instrospection<T, P>::map<0>() == 0;
predicate_instrospection<T, P>::map<1>() == 2;
predicate_instrospection<T, P>::map<...>() == compile-error;

// Go from index in the host struct to index in the filtered members
predicate_instrospection<T, P>::unmap<0>() == 0;
predicate_instrospection<T, P>::unmap<1>() == compile-error;
predicate_instrospection<T, P>::unmap<2>() == 1;
predicate_instrospection<T, P>::unmap<...>() == compile-error;
```

