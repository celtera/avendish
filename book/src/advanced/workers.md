# Workers

It is relatively common to require to perform some long-running work in a separate thread: 
processing samples, etc.

Avendish provides two APIs for doing this: a general one, and a simpler one for the specific case of pre-processing
the data of a port (for instance when a file is loaded).

The general idea is that the processor code sends a request to the worker with some data.
The host processes the request in a thread. This returns a function which is then called back 
onto the processor, to update its state with the result of the computations.

> This is currently only supported in ossia.

## General worker API Usage

In the main class, define a `worker` struct/instance ; the arguments can be anything.
Here we'll have a request that takes an `int, std::string` set of arguments ; to have multiple 
behaviours one can use a std::variant of potential requests.

```cpp
struct MyObject
{
  struct worker
  {
    // Called from DSP thread
    std::function<void(int, std::string)> request;

    // "work" is called from a worker thread
    static std::function<void(MyObject&)> work(int, std::string);

    // The std::function object returned by work is called from the DSP thread
  } worker;
};
```

Here is how to use it:

```cpp
// 1. Within the DSP thread:
void operator()(...) {
  if(something_happened) {
    this->worker.request(123, "foo");
  }
}

// 2. Implement the worker::work function:
std::function<void(MyObject&)> MyObject::worker::work(int x, std::string foo)
{
  // 3. Implement the "threaded" work part:
  // This is executed in a separate worker thread, slow operations can be done here 
  // safely.

  // Repeat the string x times:
  std::string orig = foo;
  while(x > 0) {
    foo += orig;
  }

  // 4. Return a function which will update the internal state of MyObject:
  return [str = std::move(foo)] (MyObject& obj) {
    // Executed in the DSP thread again, so don't do long operations here!
    obj.internal_string = str;
  };
}
```

The astute reader will have noticed a fairly bad performance issue in the function above: we 
are copying the entire string in the DSP thread! This is very bad.

How can that be improved?

```cpp
// Sadly does not do anything as the object inside the lambda is const by default, 
// so this is still making a copy
return [str = std::move(foo)] (MyObject& obj) {
  obj.internal_string = std::move(str);
};

// Adding mutable at least removes the copy... but there is still a performance issue!
return [str = std::move(foo)] (MyObject& obj) mutable {
  obj.internal_string = std::move(str);
};

// Replacing the string even with std::move may call `free` on `obj.internal_string` 
// which is not a real-time-safe operation.
// Instead, if the data is swapped with the string variable in the lambda, 
// a well-written host will make sure that the lambda ends its lifetime outside of 
// the DSP thread, ensuring perfectly safe real-time operation.
return [str = std::move(foo)] (MyObject& obj) mutable {
  std::swap(obj.internal_string, str);
};
```

A complete example is available here: <https://github.com/ossia/score-addon-threedim/blob/main/Threedim/StructureSynth.hpp>

## Simple threaded port-processing API usage

This API is a "simplified" version of the above one for the common case of wanting 
to pre-process some data which was loaded from the hard drive.

It is accessed by adding the following to a port:

```cpp
struct : my_file_port {
  static std::function<void(MyObject&)> process(file_type data)
  {
    // 1. Process the raw file data. This happens in a worker thread.
    int N = long_operation(data);

    // 2. Return a function that will apply the change to the object.
    return [N] (MyObject& obj) { 
      // Executed in the processing thread.
      obj.foo(N); 
    };
  }
} my_port;
```

A complete example is available here: <https://github.com/ossia/score-addon-threedim/blob/main/Threedim/ObjLoader.hpp>
