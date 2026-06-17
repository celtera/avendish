# Compile-time benchmark harness

Measures how long it takes to build avendish objects across the three aggregate
backends — **boost.pfr**, **P1061** structured-binding packs, and **C++26 static
reflection** — on GCC 16 and the clang-p2996 fork.

## Layout
- `generate_cases.py` — writes `cases/` (real examples), `scaling/` (synthetic
  flat-N / rec-N) and `micro/` (pure mechanism micro-benchmarks).
- `exercise.tmpl` — per-example TU template: instantiates the introspection a
  binding walks (input/output/messages introspection + per-port iteration +
  metadata getters), with no fmt/json so the signal is the avnd backend.
- `bench_examples.sh` / `bench_scaling.sh` / `bench_micro.sh` — in-container
  timing loops (wall clock via `date`, best-of / median, warmup dropped).
- `bench_memory.sh` — peak RSS via GNU `time`.
- `aggregate.py` — turns the per-run CSVs into per-(compiler,backend) tables.
- `results/` — baseline CSVs captured 2026-06-17 (before the flat_tie rewrite).
- `RESULTS-baseline.md` — the written analysis of the baseline run.

## Running (in tmpfs, via Docker)
```sh
B=$(pwd)/benchmarks/compile-time
python3 $B/generate_cases.py
R=$(pwd); ME=$R/build/_deps/magic_enum-src/include
# examples (median of 6), both compilers:
docker run --rm -v $R:/repo:ro -v /usr/include/boost:/usr/include/boost:ro \
  -v $ME:/me:ro -v $B:/w gcc:16 \
  bash /w/bench_examples.sh "g++" "" "-freflection" gcc16 > $B/results/results_gcc16.csv
docker run --rm -v $R:/repo:ro -v /usr/include/boost:/usr/include/boost:ro \
  -v $ME:/me:ro -v $B:/w vsavkov/clang-p2996:amd64 \
  bash /w/bench_examples.sh "clang++" "-stdlib=libc++" "-freflection-latest" clang21 > $B/results/results_clang21.csv
python3 $B/aggregate.py $B/results/results_gcc16.csv $B/results/results_clang21.csv
```
`bench_scaling.sh` / `bench_micro.sh` take the same arguments. The micro-benches
pass `-freflection*` only to TUs whose name starts with `refl_`.

## Headline baseline findings (see RESULTS-baseline.md)
- Typical objects: reflection costs ~+13-17% vs the structured-binding baseline.
- The flattening cost is super-linear in member-count at one struct level; the
  micro-benchmarks pin it to `std::tuple_cat` (≈O(N²) gcc, ≈O(N^2.5) clang/libc++).
  A single `std::tie` is near-linear — replacing the N-way `tuple_cat` saves 57%
  (gcc) / 92% (clang) of the reflection flat-tie cost.
