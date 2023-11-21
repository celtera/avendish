#pragma once

// Sadly no way to do this without a macro...
#define AVND_DEFINE_TAG(Tag) \
  template <typename T>      \
  concept tag_##Tag = requires { T::Tag; } || requires { sizeof(typename T::Tag*); };
