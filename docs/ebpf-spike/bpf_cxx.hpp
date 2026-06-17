#pragma once
// Minimal C++-clean replacement for the C-only bits of <bpf/bpf_helpers.h>.
// Two reasons we can't use libbpf's header directly from C++:
//   1. helpers are declared `static T (*const fn)(...) = (void*)N;` -- the
//      implicit void*->funcptr conversion is ill-formed in C++.
//   2. the __uint/__type map-field macros collide with libstdc++ internals
//      (e.g. __gnu_cxx::__promote<>::__type) once <utility> is included.
// So the avendish eBPF backend ships its own. BTF maps are declared with
// explicit field types instead of the macros.
#include <linux/bpf.h>

#define SEC(NAME) __attribute__((section(NAME), used))

// the handful of helpers we need, declared with proper C++ casts
static void* (* const bpf_map_lookup_elem)(void* map, const void* key)
    = reinterpret_cast<void* (*)(void*, const void*)>(1);
static long  (* const bpf_map_update_elem)(void* map, const void* key,
                                           const void* value, __u64 flags)
    = reinterpret_cast<long (*)(void*, const void*, const void*, __u64)>(2);
