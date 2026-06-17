#!/usr/bin/env bash
# Phase-0 eBPF spike build. Compiles avendish-style boost::pfr reflection +
# q15 fixed-point to BPF bytecode and (if bpftool is present) emits a skeleton.
#
# The only non-obvious part: cross-targeting `bpf` does NOT auto-add the host's
# C++ stdlib header search paths, so we discover them from the host compiler and
# feed them back in as -isystem. (Boost.PFR is header-only / compile-time, so we
# only need headers, not a target libstdc++.)
set -euo pipefail
cd "$(dirname "$0")"

CXX=${CXX:-clang++}

# Discover host C++ <...> search dirs.
mapfile -t INCS < <(printf '' | "$CXX" -x c++ -E -v - 2>&1 \
  | sed -n '/#include <...> search starts/,/End of search/p' \
  | grep '^ ' | sed 's/^[[:space:]]*//')
ISYS=(); for d in "${INCS[@]}"; do ISYS+=(-isystem "$d"); done

FLAGS=(-target bpf -O2 -g -std=c++23 -fno-exceptions -fno-rtti "${ISYS[@]}")

echo ">>> spike_pfr.cpp  (pfr -> BPF, no scaffolding)"
"$CXX" "${FLAGS[@]}" -c spike_pfr.cpp -o spike_pfr.o
echo "    $(file -b spike_pfr.o)"

echo ">>> xdp_gain.cpp   (loadable XDP: pfr + q15 + map + bounds checks)"
"$CXX" "${FLAGS[@]}" -c xdp_gain.cpp -o xdp_gain.bpf.o
echo "    $(file -b xdp_gain.bpf.o)"

echo ">>> spike_data.cpp (loadable XDP: int + fixed_string + struct + map; no FP)"
"$CXX" "${FLAGS[@]}" -c spike_data.cpp -o spike_data.bpf.o
echo "    $(file -b spike_data.bpf.o)"

if command -v bpftool >/dev/null; then
  bpftool gen skeleton xdp_gain.bpf.o name xdp_gain > xdp_gain.skel.h
  echo ">>> skeleton: xdp_gain.skel.h ($(wc -l < xdp_gain.skel.h) lines)"
fi

cat <<'EOF'

Built. Inspect:   llvm-objdump -d xdp_gain.bpf.o
Load (needs root): sudo bpftool prog loadall xdp_gain.bpf.o /sys/fs/bpf/xdp_gain
  (this invokes the in-kernel verifier -- the one step the spike could not run
   here because the sandbox lacks CAP_BPF.)
EOF
