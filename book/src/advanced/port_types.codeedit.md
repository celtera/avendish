# Code editors

> Supported bindings: ossia

For live-coding and similar purposes, it is common to embed a domain-specific programming language 
into a host environment: Faust, math expression languages, Javascript, Lisp, etc...

If one adds the `language` metadata to a string port, then the port will be recognized 
as a programming language code input: hosts are encouraged to show some relevant text editor for code 
instead of a simple line edit.

## Example

```cpp
struct : halp::lineedit<"Program", "">
{
  halp_meta(language, "INTERCAL")
} program;
```

should show up as a code editor with support for [INTERCAL](https://en.wikipedia.org/wiki/INTERCAL) programs.
