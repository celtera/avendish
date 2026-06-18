# Compile-time analysis (`-ftime-trace`) and optimization directions

Follow-up to the structured-binding flattener (PR #110). This drills into where
the remaining compile time goes, per backend, with `clang -ftime-trace`, and
records what was measured to help vs not — so we don't chase placebo fixes.

## TL;DR
- The one large, pathological cost (the recursive/array flattener) is already
  fixed in #110: it was an O(N²) `tuple_cat` **and** O(N²) re-evaluation of the
  consteval-only `nonstatic_data_members_of()`. The structured-binding rewrite
  made the reflection introspection lean (~1.5 s for a 128-port object).
- **The remaining cost on the CI compilers is `boost.pfr` itself**, not avendish
  code, and it is **not portably reducible on clang-20 / gcc-14**.
- avendish's own introspection + metadata getters are already cheap (getters:
  ~0.02 ms each).
- So the biggest remaining lever is the **backend/compiler choice**, not a
  per-translation-unit code change.

## Method
`clang++ -std=c++26 -ftime-trace -c -O0/-O1`, persistent containers (GCC 16.1,
clang-p2996/LLVM21), real example objects via the SDK-free `dump` backend and
synthetic flat-N / rec-N objects. Traces aggregated by template instantiation.

## Where the time goes — `examples/Raw/AllPortsTypes` via the `dump` backend, boost.pfr (the CI backend), 8.0 s total
| ms | n | what | controllable by avendish? |
|---:|---:|---|---|
| 3380 | 1 | `dump_cbor::dump<…>` (fmt + nlohmann/json codegen) | no — `dump`-binding only |
| ~2000 | — | `boost::pfr::detail::detect_fields_count` / `fields_count` / `tie_as_tuple` | **no — boost.pfr internals** |
| 734 | 96 | `dump_cbor::print_metadatas` (metadata getter cascade × ports) | partly |
| ~1200 | — | `input_introspection` / `fields_introspection` / `inputs_type` / `structure_to_tpltuple` | yes (introspection layer) |

i.e. on the CI backend roughly **a quarter of a real object build is boost.pfr's
own SFINAE field-counting**, and another large chunk is the binding's fmt/json.

By contrast the **reflection** backend on a json-free 128-port object is ~1.5 s
total, dominated by `fields_introspection` (211 ms) / `for_each_field_ref`
(210 ms) / `flat_tie` (135 ms) — all already lean after #110.

## The flattener win (already merged in #110), for reference
| object (reflection) | before | after (structured bindings) |
|---|---:|---:|
| flat-256-port, clang | 82.6 s | **1.8 s** |
| flat-256-port, gcc | 56 s | 45 s* |
| kabang (220 ports), clang | 9.8 s | 6.1 s |

\* gcc's residual is the per-port introspection machinery, not the flattening
(clang's is cheap, hence its 45×). boost.pfr cannot compile a flat aggregate
beyond ~64 fields at all; p1061 and reflection can.

## Measured to NOT help (so we don't waste time)
- **`__builtin_structured_binding_size`** for field counting: absent on gcc-14,
  not usable on clang-20 (`__has_builtin` = false). Only clang-21+ has it — and
  there avendish already uses the P1061 backend, which doesn't need it.
- **`__make_integer_seq` / `__integer_pack`**: libstdc++ and libc++ already route
  `std::make_index_sequence` through these builtins on both CI compilers; calling
  the builtin directly only skips libc++'s thin offset wrapper (sub-0.1 s class).
- **Metadata getters / the `[[=halp::meta]]` annotation branch**: ~0.02 ms per
  getter; the reflection migration added no measurable cost to the non-reflection
  CI path (header-only `metadatas.hpp` 0.369 s vs +1200 getters 0.39 s).

## Recommended directions (in impact order)
1. **Move the CI toolchains to clang ≥ 21 / gcc ≥ 16.** They auto-select the
   P1061 backend, which eliminates boost.pfr's SFINAE field-counting (the ~25 %
   chunk above) outright — the single biggest realistic win, and zero source
   risk. clang-20 / gcc-14 are stuck on boost.pfr (no P1061 packs, no
   structured-binding-size builtin), so no source change can remove that cost
   for them.
2. **Unify the structured-binding flattener across the P1061 and reflection
   backends** (code change, all P1061/reflection compilers): the recursion +
   port-array flattening from #110 is pure structured bindings and needs no
   `std::meta`, so the P1061 backend can share it — giving clang ≥ 21 / gcc 16
   `recursive_group` + port-array support (which P1061 lacks today: it currently
   fails `test_introspection_rec`) at the same low cost. Feature parity + DRY;
   not a CI-time win until (1).
3. **Consolidate redundant tuple construction in the introspection layer**
   (deeper, all backends): `tie_as_tuple`, `structure_to_tpltuple`,
   `structure_tpltie`, `structure_to_tuple` each re-tie the same struct; a real
   object build shows boost.pfr counting/tying the same aggregate several times.
   Deriving them from one tie would cut repeated boost.pfr instantiations — but
   it touches the core and needs careful per-backend validation.

## What this PR contains
This branch carries the analysis only; (2) and (3) are follow-up code changes to
land once we agree on direction (and, for (1), once the CI images move forward).
