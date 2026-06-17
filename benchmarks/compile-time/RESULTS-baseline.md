# Avendish compile-time benchmark — boost.pfr vs P1061 vs C++26 reflection

Method: each object compiled `-std=c++26 -c -O0` (isolates front-end/template cost
where backends differ). One persistent container per compiler (no per-compile docker
overhead). Wall-clock via `date +%s.%N`; median of 6 runs (warmup dropped) for the
real objects, best-of-3 for the synthetic scaling series. Peak RSS via GNU `time`.
Compilers: gcc 16.1 (`-freflection`), clang-p2996/LLVM21 (`-freflection-latest`,
libc++). Backends: boost.pfr (`-DAVND_USE_BOOST_PFR=1`), P1061 packs (default),
reflection (`-freflection*`). Workload = a TU instantiating the introspection a
binding walks (input/output/messages introspection + per-port iteration + metadata),
no fmt/json. 11 real examples + synthetic flat-N / rec-N series.

## Typical objects (10 small/medium real examples) — mean per object
                pfr      p1061    refl     refl vs p1061
  gcc 16       1.03s    1.00s    1.16s    +17%
  clang21      0.98s    0.95s    1.07s    +13%
Fixed baseline ~0.9-1.0s (headers+introspection). Reflection adds ~13-17%. Fine.
Peak RSS (medium obj): gcc ~315 MB, clang ~170 MB — same across all 3 backends.

## Large recursive object (kabang, ~220 flattened ports)
                pfr       p1061*    refl
  gcc 16       4.54s     1.08s     36.2s     (refl 8.0x slower than pfr)
  clang21      16.9s     1.04s      9.8s     (refl 1.7x FASTER than pfr)
  *p1061 doesn't flatten recursive groups -> not equivalent work, ~constant.
Peak RSS kabang: gcc pfr 667 / refl 607 MB; clang pfr 1021 / refl 948 MB.
=> Reflection's cost here is TIME (slow std::meta constant-eval), not memory
   (it uses slightly LESS RAM than boost.pfr). Compilers diverge wildly.

## Scaling law (synthetic, best-of-3 s)
  FLAT (N ports directly in one struct):
            gcc:refl   clang:refl   (p1061 ~0.9s flat; pfr fails >~64 fields)
   N=16      1.20        1.25
   N=64      2.85        4.20
   N=128    10.19       16.10
   N=256    56.06       82.62        <- ~O(N^2.3): doubling N ~= 3.5-5x time
  RECURSIVE (N leaves via 8-wide groups):
   N=64      1.70        1.42
   N=128     5.41        2.58        (clang refl ~= pfr here)
   N=256    32.08        8.81        (clang refl < pfr 9.49)

## Findings
1. Normal objects: reflection ~ +13-17% vs the structured-binding baseline. Acceptable.
2. p1061 is the fastest backend but CANNOT flatten recursive_group / arrays
   (constant ~0.9s because it does less work). Not a like-for-like comparison.
3. boost.pfr cannot compile flat aggregates beyond ~64-100 fields (hard limit);
   reflection and p1061 handle 256+ fine.
4. The flattening cost is SUPER-LINEAR in the member count *at one struct level*.
   The bottleneck is the single N-wide `flat_tie_impl<info... Ms>` + N-way
   `tpl::tuple_cat`. Hierarchy (groups) splits it into small chunks and scales far
   better -> real grouped objects are OK; a flat 100+ port object is pathological,
   worst on GCC.
5. clang-p2996 evaluates std::meta MUCH faster than GCC at scale (kabang 9.8 vs 36s)
   but uses ~2x the RAM.

## Optimization opportunities (reflection flattener)
- Replace the single wide `tuple_cat` with a balanced/divide-and-conquer cat, or
  build the flat tuple by indexed splice instead of tuple_cat.
- Memoize per-type member-info (consteval queries re-run each call; cache into a
  type-level constant) to cut GCC's constant-eval blowup.

## Micro-benchmark validation (isolating the root cause before fixing)
Controlled TUs, no avnd/halp — only std::tuple + (optionally) std::meta. best-of-3, -O0 -c.

N-way build of a tuple of N references, scaling N = 32/64/128/256 (seconds):
                 gcc16                         clang21
              32   64  128  256            32   64   128   256
 puretie    0.12 0.16 0.28 0.65          0.06 0.08 0.11  0.18    single std::tie (1 pack)
 purecat    0.17 0.36 1.04 4.22          0.54 2.13 10.2 57.10    N-way std::tuple_cat
 refl_enum  0.26 0.31 0.51 1.30          0.49 0.72 1.54  4.75    reflect+feed, no tuple
 refl_tie   0.31 0.45 1.04 3.52          0.53 0.76 1.61  4.90    reflect + single tie
 refl_cat   0.39 0.72 2.17 8.20          0.98 2.82 11.8 62.00    reflect + N-way tuple_cat

VALIDATED CLAIMS:
1. std::tuple_cat is the culprit, and it is SUPER-LINEAR in arg count:
   - gcc:   ~O(N^2)   (x2.9 then x4.0 per doubling)
   - clang: ~O(N^2.5) (x4.8 then x5.6 per doubling) -- catastrophic on libc++
   A single std::tie (one pack expansion) is near-LINEAR on both.
   tuple_cat / tie at N=256:  gcc 6.5x,  clang 324x.
2. Replacing the N-way tuple_cat with a single tie in the reflection path
   (refl_cat -> refl_tie) saves 57% on gcc (8.2->3.5s) and 92% on clang (62->4.9s).
   => The fix target is "flatten to one pack + tie once", NOT tuple_cat.
3. Reflection ENUMERATION is NOT the bottleneck: refl_enum is near-linear and
   small, and refl_tie ~= refl_enum (the tie adds little once members are fed).
   => Memoizing consteval member-queries is at best a SECONDARY win; do the
      tuple_cat->tie rewrite first. (This corrects the earlier guess that
      ranked memoization equally.)
4. A naive "balanced" cat (make_tuple of ties then one tuple_cat) does NOT help
   (purebal >= purecat). The win is specifically the single std::tie.
